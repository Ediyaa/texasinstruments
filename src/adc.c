#include <msp430.h>
#include <math.h>
#include "include/adc.h"

/* ---------- interner Zustand ---------- */

static volatile uint16_t s_value;      /* letzter Wert aus ADC12MEM0 */
static volatile bool     s_new_value;  /* true, wenn noch nicht abgeholt */

/* Messblock für adcBlockCapture. NOINIT: die 4 KB werden beim Start nicht genullt */
#pragma NOINIT(s_block)
static uint16_t s_block[ADC_BLOCK_LEN];

/* ---------- Initialisierung ---------- */

/* ADC12_A für Einzelwerte konfigurieren, Start durch TB0.1 (Normalbetrieb) */
static void adc_config_periodic(void)
{
    /* ADC12_A sperren, damit die Konfiguration geändert werden darf */
    ADC12CTL0 &= ~ADC12ENC;

    /* ADC12CTL0:
       Bitfeld ADC12SHT0x = 0001 -> Abtastzeit 8 Takte ADC12CLK
       Bit ADC12ON        = 1    -> ADC einschalten
       Bit ADC12MSC       = 0    -> jede Umsetzung braucht eine eigene Flanke */
    ADC12CTL0 = ADC12SHT0_1 | ADC12ON;

    /* ADC12CTL1:
       Bitfeld ADC12CSTARTADDx = 0  -> Ergebnis in ADC12MEM0
       Bitfeld ADC12SHSx       = 11 -> Startimpuls von TB0.1
       Bit ADC12SHP            = 1  -> Abtastzeit vom Abtast-Timer (ADC12SHT0x)
       Bitfeld ADC12DIVx       = 0  -> Teiler 1
       Bitfeld ADC12SSELx      = 00 -> ADC12OSC
       Bitfeld ADC12CONSEQx    = 10 -> Einzelkanal wiederholt */
    ADC12CTL1 = ADC12CSTARTADD_0 | ADC12SHS_3 | ADC12SHP |
                ADC12DIV_0 | ADC12SSEL_0 | ADC12CONSEQ_2;

    /* ADC12CTL2:
       Bitfeld ADC12RESx = 10 -> 12 Bit */
    ADC12CTL2 = ADC12RES_2;

    /* ADC12MCTL0:
       Bitfeld ADC12SREFx = 000  -> V_R+ = AV_CC, V_R- = AV_SS (ratiometrisch)
       Bitfeld ADC12INCHx = 0000 -> Kanal A0 (P6.0) */
    ADC12MCTL0 = ADC12SREF_0 | ADC12INCH_0;

    /* Interrupt bei fertigem Ergebnis in ADC12MEM0 */
    ADC12IFG = 0;
    ADC12IE  = ADC12IE0;

    /* ADC freigeben; ab jetzt startet jede steigende Flanke von TB0.1 eine Umsetzung */
    ADC12CTL0 |= ADC12ENC;
}

void adcInit(void)
{
    /* P6.0 als analoger Eingang A0 */
    P6SEL |= BIT0;
    P6DIR &= ~BIT0;

    adc_config_periodic();

    /* Timer TB0:
       TB0CCR0 -> Periode, TB0CCR1 -> Flanke in der Periodenmitte
       Ausgabemodus 7 (Reset/Set): steigende Flanke bei Periodenbeginn */
    TB0CCR0  = (uint16_t)ADC_TIMER_PERIOD;
    TB0CCR1  = (uint16_t)(ADC_TIMER_PERIOD / 2UL);
    TB0CCTL1 = OUTMOD_7;

    /* Bitfeld TBSSELx = 01 -> ACLK, Bitfeld MCx = 01 -> Aufwärtszählen bis TB0CCR0 */
    TB0CTL = TBSSEL_1 | MC_1 | TBCLR;
}

/* ---------- Abholen ---------- */

bool adcGet(uint16_t *n)
{
    bool available;
    uint16_t sr = __get_SR_register();   /* Interruptzustand merken */

    __disable_interrupt();
    available = s_new_value;
    if (available) {
        *n = s_value;
        s_new_value = false;
    }
    __bis_SR_register(sr & GIE);         /* nur wieder freigeben, wenn vorher frei */

    return available;
}

uint16_t adcLast(void)
{
    return s_value;     /* 16-Bit-Lesezugriff ist auf dem MSP430 atomar */
}

/* ---------- Übersetzung ---------- */

/* V_in = N * AV_CC / 4095, kaufmännisch gerundet */
uint16_t adcToMillivolt(uint16_t n)
{
    return (uint16_t)(((uint32_t)n * ADC_AVCC_MV + 4095UL / 2UL) / 4095UL);
}

/* R_T unten: R_T = R_1 * N / (4095 - N)
   R_T oben:  R_T = R_1 * (4095 - N) / N
   AV_CC kürzt sich heraus, kaufmännisch gerundet */
uint32_t adcToOhm(uint16_t n)
{
    uint32_t zaehler;
    uint32_t nenner;

#if ADC_RT_LOW_SIDE
    zaehler = ADC_R1_OHM * (uint32_t)n;
    nenner  = 4095UL - (uint32_t)n;
#else
    zaehler = ADC_R1_OHM * (4095UL - (uint32_t)n);
    nenner  = (uint32_t)n;
#endif

    if (nenner == 0UL) {
        return ADC_OHM_INVALID;
    }

    return (zaehler + nenner / 2UL) / nenner;
}

/* Steinhart-Hart: 1/T = A + B * ln(R_T) + C * (ln(R_T))^3,
   Ergebnis in 0,01 °C, kaufmännisch gerundet */
int32_t adcToCentiCelsius(uint16_t n)
{
    uint32_t r = adcToOhm(n);
    float    l;
    float    t;

    if (r == ADC_OHM_INVALID || r == 0UL) {
        return ADC_TEMP_INVALID;
    }

    l = logf((float)r);
    t = 1.0f / (ADC_SH_A + ADC_SH_B * l + ADC_SH_C * l * l * l);   /* in K */
    t = (t - 273.15f) * 100.0f;                                      /* in 0,01 °C */

    return (int32_t)((t >= 0.0f) ? (t + 0.5f) : (t - 0.5f));
}

/* ---------- Blockmessung ---------- */

/* ADC_BLOCK_LEN Werte am Stück aufnehmen, blockiert bis der Block voll ist.
   ADC12_A läuft frei (ADC12MSC = 1), jede Umsetzung startet sofort die nächste:
   f_A = f_ADC12OSC / (8 + 13 + 1 Takte) = ca. 200 ... 230 kHz.
   DMA-Kanal 0 kopiert jeden Wert von ADC12MEM0 nach s_block[i]. */
void adcBlockCapture(void)
{
    uint16_t ie = ADC12IE;

    ADC12CTL0 &= ~ADC12ENC;

    /* DMA wird von ADC12IFG0 nur ausgelöst, wenn ADC12IE0 = 0 ist (SLAU208Q, 28.2.10) */
    ADC12IE  = 0;
    ADC12IFG = 0;

    /* ADC12CTL0:
       Bitfeld ADC12SHT0x = 0001 -> Abtastzeit 8 Takte ADC12CLK
       Bit ADC12MSC       = 1    -> nächste Umsetzung startet automatisch
       Bit ADC12ON        = 1    -> ADC einschalten */
    ADC12CTL0 = ADC12SHT0_1 | ADC12MSC | ADC12ON;

    /* ADC12CTL1: wie im Normalbetrieb, aber
       Bitfeld ADC12SHSx = 00 -> Start durch ADC12SC (Software) */
    ADC12CTL1 = ADC12CSTARTADD_0 | ADC12SHS_0 | ADC12SHP |
                ADC12DIV_0 | ADC12SSEL_0 | ADC12CONSEQ_2;

    /* DMACTL0: Bitfeld DMA0TSELx = 24 -> Auslöser ADC12IFGx (MSP430F5529) */
    DMACTL0 = (DMACTL0 & 0xFFE0u) | DMA0TSEL_24;

    /* Quelle ADC12MEM0 fest, Ziel s_block, Anzahl ADC_BLOCK_LEN */
    __data16_write_addr((unsigned short)&DMA0SA, (unsigned long)&ADC12MEM0);
    __data16_write_addr((unsigned short)&DMA0DA, (unsigned long)&s_block[0]);
    DMA0SZ = ADC_BLOCK_LEN;

    /* DMA0CTL:
       Bitfeld DMADTx      = 000 -> Einzelübertragung je Auslöser
       Bitfeld DMADSTINCRx = 11  -> Zieladresse erhöhen
       Bitfeld DMASRCINCRx = 00  -> Quelladresse fest
       Bit DMAEN           = 1   -> Kanal freigeben (wird nach DMA0SZ Übertragungen gelöscht) */
    DMA0CTL = DMADT_0 | DMADSTINCR_3 | DMASRCINCR_0 | DMAEN;

    /* Erste Umsetzung per Software starten */
    ADC12CTL0 |= ADC12ENC | ADC12SC;

    while (DMA0CTL & DMAEN) {
        /* warten, bis der Block voll ist */
    }

    /* Wiederholbetrieb stoppt am Ende der laufenden Umsetzung (SLAU208Q, 28.2.7.6) */
    ADC12CTL0 &= ~ADC12ENC;
    while (ADC12CTL1 & ADC12BUSY) {
    }
    DMA0CTL &= ~DMAIFG;

    /* Normalbetrieb wiederherstellen */
    adc_config_periodic();
    ADC12IE = ie;
}

/* Arithmetischer Mittelwert über den Block, kaufmännisch gerundet */
uint16_t adcBlockMean(void)
{
    uint32_t sum = 0UL;
    uint16_t i;

    for (i = 0u; i < ADC_BLOCK_LEN; i++) {
        sum += s_block[i];
    }

    return (uint16_t)((sum + ADC_BLOCK_LEN / 2UL) / ADC_BLOCK_LEN);
}

/* Rekursiver Tiefpass erster Ordnung über den Block:
     y[n] = y[n-1] + (x[n] - y[n-1]) / 2^K,  K = ADC_LP_SHIFT
   y wird mit 2^K skaliert geführt (K Nachkommabits), Start mit dem ersten Wert.
   Ergebnis ist der letzte Ausgangswert, kaufmännisch gerundet */
uint16_t adcBlockLowpass(void)
{
    uint32_t y = (uint32_t)s_block[0] << ADC_LP_SHIFT;
    uint16_t i;

    for (i = 1u; i < ADC_BLOCK_LEN; i++) {
        y = y - (y >> ADC_LP_SHIFT) + (uint32_t)s_block[i];
    }

    return (uint16_t)((y + (1UL << (ADC_LP_SHIFT - 1u))) >> ADC_LP_SHIFT);
}

/* ---------- Interruptroutine ---------- */

#pragma vector = ADC12_VECTOR
__interrupt void adc12_isr(void)
{
    switch (__even_in_range(ADC12IV, 34)) {
    case 6:                           /* ADC12IFG0 */
        s_value     = ADC12MEM0;      /* Lesen löscht ADC12IFG0 */
        s_new_value = true;
        break;
    default:
        break;
    }
}
