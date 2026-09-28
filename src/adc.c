#include <msp430.h>
#include "include/adc.h"

/* ---------- interner Zustand ---------- */

static volatile uint16_t s_value;      /* letzter Wert aus ADC12MEM0 */
static volatile bool     s_new_value;  /* true, wenn noch nicht abgeholt */

/* ---------- Initialisierung ---------- */

void adcInit(void)
{
    /* P6.0 als analoger Eingang A0 */
    P6SEL |= BIT0;
    P6DIR &= ~BIT0;

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
