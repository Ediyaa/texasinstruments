#include <msp430.h>
#include <math.h>
#include "include/adc.h"

/* ---------- interner Zustand ---------- */

static volatile uint16_t s_value;      /* letzter Wert aus ADC12MEM0 */
static volatile bool     s_new_value;  /* true, wenn noch nicht abgeholt */

/* Messblock für adcBlockCapture. NOINIT: die 4 KB werden beim Start nicht genullt */
#pragma NOINIT(s_block)
static uint16_t s_block[ADC_BLOCK_LEN];

/* ---------- Laufende Filter: Zustand ---------- */

/* Zustand eines laufenden Filters (gleitender Mittelwert und Tiefpass) */
typedef struct {
    uint16_t buf[ADC_FILT_MEAN_LEN];  /* letzte Rohwerte (Ringspeicher)               */
    uint32_t sum;                     /* Summe über buf                                */
    uint16_t idx;                     /* nächste Schreibstelle in buf                  */
    uint32_t y;                       /* Tiefpass-Ausgang, mit 2^ADC_FILT_LP_SHIFT skaliert */
    bool     started;                 /* false: nächster Wert startet den Filter neu   */
} adc_filter_t;

static volatile bool s_mean_on    = false;   /* Befehl adcmean    */
static volatile bool s_lowpass_on = false;   /* Befehl adclowpass */

static adc_filter_t s_filt_stream;           /* Normalbetrieb (Interruptroutine) */
static adc_filter_t s_filt_hist;             /* Histogramm                       */

/* Einen Rohwert x filtern: x -> gleitender Mittelwert (falls an) -> Tiefpass (falls an).
   Beim ersten Wert nach einem Neustart werden Ringspeicher und Tiefpass mit x
   gefüllt, damit der Ausgang nicht von 0 aus einschwingt */
static uint16_t adc_filter(adc_filter_t *f, uint16_t x)
{
    uint16_t v = x;
    uint16_t i;

    if (!f->started) {
        for (i = 0u; i < ADC_FILT_MEAN_LEN; i++) {
            f->buf[i] = x;
        }
        f->sum     = (uint32_t)x * ADC_FILT_MEAN_LEN;
        f->idx     = 0u;
        f->y       = (uint32_t)x << ADC_FILT_LP_SHIFT;
        f->started = true;
    }

    if (s_mean_on) {
        f->sum = f->sum - f->buf[f->idx] + x;
        f->buf[f->idx] = x;
        if (++f->idx >= ADC_FILT_MEAN_LEN) {
            f->idx = 0u;
        }
        v = (uint16_t)((f->sum + ADC_FILT_MEAN_LEN / 2u) / ADC_FILT_MEAN_LEN);
    }

    if (s_lowpass_on) {
        /* y wird mit 2^K skaliert geführt: y = y - y / 2^K + v.
           y / 2^K gerundet statt abgeschnitten, sonst liegt der Ausgang
           im Mittel bis zu eine Stufe zu hoch */
        f->y = f->y - ((f->y + (1UL << (ADC_FILT_LP_SHIFT - 1u))) >> ADC_FILT_LP_SHIFT) + v;
        v = (uint16_t)((f->y + (1UL << (ADC_FILT_LP_SHIFT - 1u))) >> ADC_FILT_LP_SHIFT);
    }

    return v;
}

/* Filter des Normalbetriebs neu starten. Aufruf aus dem Hauptprogramm,
   Interrupt dafür kurz gesperrt */
static void adc_filter_restart_stream(void)
{
    uint16_t sr = __get_SR_register();

    __disable_interrupt();
    s_filt_stream.started = false;
    __bis_SR_register(sr & GIE);
}

/* ---------- Initialisierung ---------- */

/* ADC12_A sofort anhalten, egal in welchem Zustand die Ablaufsteuerung ist:
   ADC12ENC = 0, danach ADC12CONSEQx = 0 (erst jetzt änderbar) und ADC12ON = 0.
   Wartet begrenzt, bis ADC12BUSY = 0. Das Ergebnis einer abgebrochenen
   Umsetzung ist ungültig, alle Flags werden gelöscht. */
static void adc_stop(void)
{
    uint16_t timeout = 1000u;

    ADC12CTL0 &= ~ADC12ENC;
    ADC12CTL1 &= ~ADC12CONSEQ_3;
    ADC12CTL0 &= ~ADC12ON;

    while ((ADC12CTL1 & ADC12BUSY) && --timeout) {
    }

    ADC12IFG = 0;
}

/* Interne Referenz einschalten und Einschwingen abwarten.
   Bit REFMSTR -> REF-Modul steuert die Referenz, Bitfeld REFVSELx -> Spannung,
   Bit REFON -> Referenz ein, Bit REFOUT = 0 -> nur intern, nicht an P5.0 */
static void adc_ref_on(void)
{
    REFCTL0 = REFMSTR | ADC_REFVSEL | REFON;
    __delay_cycles(ADC_REF_SETTLE_CYCLES);
}

/* Timer TB0 neu starten: Quelle tbssel (Bitfeld TBSSELx), Periode in Takten.
   TB0CCR0 -> Periode, TB0CCR1 -> Flanke in der Periodenmitte,
   Ausgabemodus 7 (Reset/Set): steigende Flanke von TB0.1 bei Periodenbeginn */
static void adc_timer_start(uint16_t tbssel, uint16_t period)
{
    TB0CTL   = TBCLR;                 /* Bitfeld MCx = 00 -> angehalten */
    TB0CCR0  = (uint16_t)(period - 1u);
    TB0CCR1  = (uint16_t)((period - 1u) / 2u);
    TB0CCTL1 = OUTMOD_7;

    /* Bitfeld MCx = 01 -> Aufwärtszählen bis TB0CCR0 */
    TB0CTL   = tbssel | MC_1 | TBCLR;
}

/* ADC12_A für Einzelwerte konfigurieren, Start durch TB0.1 (Normalbetrieb) */
static void adc_config_periodic(void)
{
    /* ADC12_A anhalten, damit die Konfiguration geändert werden darf */
    adc_stop();

    /* ADC12CTL0:
       Bitfeld ADC12SHT0x = ADC_SHT0 -> Abtastzeit (adc.h)
       Bit ADC12ON        = 1        -> ADC einschalten
       Bit ADC12MSC       = 0        -> jede Umsetzung braucht eine eigene Flanke */
    ADC12CTL0 = ADC_SHT0 | ADC12ON;

    /* ADC12CTL1:
       Bitfeld ADC12CSTARTADDx = 0        -> Ergebnis in ADC12MEM0
       Bitfeld ADC12SHSx       = 11       -> Startimpuls von TB0.1
       Bit ADC12SHP            = 1        -> Abtastzeit vom Abtast-Timer (ADC12SHT0x)
       Bitfeld ADC12DIVx       = ADC_DIV  -> Teiler (adc.h)
       Bitfeld ADC12SSELx      = ADC_SSEL -> Taktquelle (adc.h)
       Bitfeld ADC12CONSEQx    = 10       -> Einzelkanal wiederholt */
    ADC12CTL1 = ADC12CSTARTADD_0 | ADC12SHS_3 | ADC12SHP |
                ADC_DIV | ADC_SSEL | ADC12CONSEQ_2;

    /* ADC12CTL2:
       Bitfeld ADC12RESx = 10 -> 12 Bit */
    ADC12CTL2 = ADC12RES_2;

    /* ADC12MCTL0:
       Bitfeld ADC12SREFx = 001  -> V_R+ = VREF+ (intern), V_R- = AV_SS
       Bitfeld ADC12INCHx = 0000 -> Kanal A0 (P6.0) */
    ADC12MCTL0 = ADC12SREF_1 | ADC12INCH_0;

    /* Interrupt bei fertigem Ergebnis in ADC12MEM0 */
    ADC12IFG = 0;
    ADC12IE  = ADC12IE0;

    /* ADC freigeben; ab jetzt startet jede steigende Flanke von TB0.1 eine Umsetzung */
    ADC12CTL0 |= ADC12ENC;
}

/* ADC12_A für Einzelumsetzungen per Software konfigurieren (Histogramm).
   Takt, Abtastzeit, Referenz und Kanal wie im Normalbetrieb */
static void adc_config_single(void)
{
    adc_stop();

    ADC12CTL0 = ADC_SHT0 | ADC12ON;

    /* ADC12CTL1: wie im Normalbetrieb, aber
       Bitfeld ADC12SHSx    = 00 -> Start durch Bit ADC12SC
       Bitfeld ADC12CONSEQx = 00 -> Einzelkanal, Einzelumsetzung */
    ADC12CTL1 = ADC12CSTARTADD_0 | ADC12SHS_0 | ADC12SHP |
                ADC_DIV | ADC_SSEL | ADC12CONSEQ_0;

    ADC12CTL2  = ADC12RES_2;
    ADC12MCTL0 = ADC12SREF_1 | ADC12INCH_0;

    ADC12IFG = 0;
    ADC12CTL0 |= ADC12ENC;
}

/* Eine Umsetzung per Bit ADC12SC starten und das Ergebnis abwarten.
   Setzt ADC12IE0 = 0 voraus, sonst holt die Interruptroutine das
   Ergebnis ab und die Schleife endet nie */
static uint16_t adc_read_single(void)
{
    ADC12CTL0 |= ADC12SC;
    while (!(ADC12IFG & ADC12IFG0)) {
    }
    return ADC12MEM0;                 /* Lesen löscht ADC12IFG0 */
}

void adcInit(void)
{
    /* P6.0 als analoger Eingang A0 */
    P6SEL |= BIT0;
    P6DIR &= ~BIT0;

    /* interne Referenz vor dem ersten Einschalten des ADC */
    adc_ref_on();

    adc_config_periodic();

    /* Timer TB0: Bitfeld TBSSELx = 01 -> ACLK, Periode für ADC_SAMPLE_RATE_HZ */
    adc_timer_start(TBSSEL_1, (uint16_t)(ADC_TIMER_PERIOD + 1UL));
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

/* V_in = N * V_R+ / 4095 (SLAU208Q, 28.2.1), kaufmännisch gerundet */
uint16_t adcToMillivolt(uint16_t n)
{
    return (uint16_t)(((uint32_t)n * ADC_VREF_MV + 4095UL / 2UL) / 4095UL);
}

/* Teiler an V_CC, gemessen gegen V_R+:  V_in = N * V_R+ / 4095
   R_T unten: R_T = R_1 * N * V_R+ / (4095 * V_CC - N * V_R+)
   R_T oben:  R_T = R_1 * (4095 * V_CC - N * V_R+) / (N * V_R+)
   V_CC = ADC_VCC_MV (Nennwert), V_R+ = ADC_VREF_MV, kaufmännisch gerundet.
   64 Bit, weil R_1 * 4095 * V_CC über 32 Bit hinausgeht */
uint32_t adcToOhm(uint16_t n)
{
    uint64_t u_in  = (uint64_t)n * ADC_VREF_MV;     /* 4095 * V_in in mV */
    uint64_t u_ges = 4095ULL * ADC_VCC_MV;           /* 4095 * V_CC in mV */
    uint64_t zaehler;
    uint64_t nenner;

    if (u_in >= u_ges) {
        return ADC_OHM_INVALID;                      /* V_in >= V_CC */
    }

#if ADC_RT_LOW_SIDE
    zaehler = (uint64_t)ADC_R1_OHM * u_in;
    nenner  = u_ges - u_in;
#else
    zaehler = (uint64_t)ADC_R1_OHM * (u_ges - u_in);
    nenner  = u_in;
#endif

    if (nenner == 0ULL) {
        return ADC_OHM_INVALID;
    }

    return (uint32_t)((zaehler + nenner / 2ULL) / nenner);
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

/* ---------- Histogramm: Hilfsfunktionen ---------- */

/* Histogramm leeren, Fenster um center legen */
static void adc_hist_reset(adcHist_t *h, uint16_t center)
{
    uint16_t i;

    h->low   = (center > ADC_HIST_HALF) ? (uint16_t)(center - ADC_HIST_HALF) : 0u;
    h->min   = 0xFFFFu;
    h->max   = 0u;
    h->below = 0u;
    h->above = 0u;
    h->n       = 0UL;
    h->sum     = 0UL;
    h->sum_raw = 0UL;
    for (i = 0u; i < ADC_HIST_BINS; i++) {
        h->count[i] = 0u;
    }
}

/* Einen Wert n ins Histogramm zählen */
static void adc_hist_count(adcHist_t *h, uint16_t n)
{
    h->n++;
    h->sum += n;

    if (n < h->min) h->min = n;
    if (n > h->max) h->max = n;

    if (n < h->low) {
        h->below++;
    }
    else if (n > (uint16_t)(h->low + ADC_HIST_BINS - 1u)) {
        h->above++;
    }
    else {
        h->count[n - h->low]++;
    }
}

/* ---------- Blockmessung ---------- */

/* Stufen der Blockabtastung: Rate und Abtastzeit (adc.h) */
typedef struct {
    uint32_t rate_hz;                 /* Abtastrate in Hz                  */
    uint16_t sht;                     /* Bitfeld ADC12SHT0x (Abtastzeit)   */
} adc_blk_step_t;

static const adc_blk_step_t s_blk_steps[3] = {
    { ADC_BLK_RATE1_HZ, ADC_BLK_SHT1 },
    { ADC_BLK_RATE2_HZ, ADC_BLK_SHT2 },
    { ADC_BLK_RATE3_HZ, ADC_BLK_SHT3 }
};

static uint16_t s_blk_step = ADC_BLK_RATE_DEFAULT;   /* eingestellte Stufe 1 ... 3 */
static uint16_t s_lp_shift = ADC_LP_SHIFT;           /* K des Blocktiefpasses      */

/* Periode von TB0 in SMCLK-Takten für die eingestellte Stufe, gerundet */
static uint16_t adc_blk_period(void)
{
    uint32_t f = s_blk_steps[s_blk_step - 1u].rate_hz;

    return (uint16_t)((ADC_BLK_SMCLK_HZ + f / 2UL) / f);
}

bool adcBlockRateSet(uint16_t step)
{
    if (step < 1u || step > 3u) {
        return false;
    }
    s_blk_step = step;
    return true;
}

uint16_t adcBlockRateGet(void)
{
    return s_blk_step;
}

bool adcLpShiftSet(uint16_t k)
{
    if (k < ADC_LP_SHIFT_MIN || k > ADC_LP_SHIFT_MAX) {
        return false;
    }
    s_lp_shift = k;
    return true;
}

uint16_t adcLpShiftGet(void)
{
    return s_lp_shift;
}

/* Tatsächliche Rate = SMCLK / Periode, gerundet */
uint32_t adcBlockRateHz(void)
{
    uint32_t p = adc_blk_period();

    return (ADC_BLK_SMCLK_HZ + p / 2UL) / p;
}

/* ADC_BLOCK_LEN Werte am Stück aufnehmen, blockiert bis der Block voll ist.
   Timer TB0 läuft dafür an SMCLK mit der eingestellten Rate, jede steigende
   Flanke von TB0.1 startet eine Umsetzung. DMA-Kanal 0 kopiert jeden Wert
   von ADC12MEM0 nach s_block[i], der Prozessorkern ist pro Wert nicht beteiligt.
   Danach laufen wieder Normalbetrieb und TB0 an ACLK.
   Rückgabe false, wenn der Block nicht innerhalb des Timeouts voll wurde. */
bool adcBlockCapture(void)
{
    uint16_t ie     = ADC12IE;
    uint16_t period = adc_blk_period();
    /* Timeout in Durchläufen der Warteschleife: Blockdauer in MCLK-Takten / 2
       (MCLK = SMCLK, clk.h). Bei mindestens 4 Takten je Durchlauf wartet
       die Schleife mindestens doppelt so lange, wie der Block dauert */
    uint32_t timeout = ((uint32_t)ADC_BLOCK_LEN * period) / 2UL;
    bool     ok;

    /* DMA wird von ADC12IFG0 nur ausgelöst, wenn ADC12IE0 = 0 ist (SLAU208Q, 28.2.10) */
    ADC12IE = 0;

    /* Normalbetrieb vollständig beenden, bevor umkonfiguriert wird */
    adc_stop();
    TB0CTL = TBCLR;                   /* Bitfeld MCx = 00 -> Timer angehalten */

    /* ADC12CTL0:
       Bitfeld ADC12SHT0x = Abtastzeit der Stufe (adc.h)
       Bit ADC12MSC       = 0 -> jede Umsetzung braucht eine eigene Flanke
       Bit ADC12ON        = 1 -> ADC einschalten */
    ADC12CTL0 = s_blk_steps[s_blk_step - 1u].sht | ADC12ON;

    /* ADC12CTL1: Takt wie im Normalbetrieb,
       Bitfeld ADC12SHSx    = 11 -> Startimpuls von TB0.1
       Bitfeld ADC12CONSEQx = 10 -> Einzelkanal wiederholt */
    ADC12CTL1 = ADC12CSTARTADD_0 | ADC12SHS_3 | ADC12SHP |
                ADC_DIV | ADC_SSEL | ADC12CONSEQ_2;

    ADC12CTL2  = ADC12RES_2;
    ADC12MCTL0 = ADC12SREF_1 | ADC12INCH_0;

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

    /* DMA-Kanal 0 wird nur von einer steigenden Flanke von ADC12IFG0 ausgelöst.
       Das Flag muss daher direkt vor dem Start 0 sein */
    ADC12IFG = 0;
    ADC12CTL0 |= ADC12ENC;

    /* Timer an SMCLK starten (Bitfeld TBSSELx = 10), ab jetzt eine Umsetzung je Periode */
    adc_timer_start(TBSSEL_2, period);

    while ((DMA0CTL & DMAEN) && --timeout) {
        /* warten, bis der Block voll ist */
    }

    ok = ((DMA0CTL & DMAEN) == 0u);

    /* ADC anhalten, DMA-Kanal 0 abschalten (falls Timeout) */
    adc_stop();
    DMA0CTL &= ~(DMAEN | DMAIFG);

    /* Normalbetrieb wiederherstellen, dessen Filter neu starten
       (Interruptroutine ist hier noch gesperrt) */
    s_filt_stream.started = false;
    adc_config_periodic();
    adc_timer_start(TBSSEL_1, (uint16_t)(ADC_TIMER_PERIOD + 1UL));
    ADC12IE = ie;

    return ok;
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

/* Tiefpass erster Ordnung über den Block, jeden Ausgangswert ins Histogramm zählen:
     y[n] = y[n-1] + (x[n] - y[n-1]) / 2^K,  K = s_lp_shift (adclptau_)
   y wird mit 2^K skaliert geführt, y / 2^K gerundet (abgeschnitten läge der
   Ausgang im Mittel zu hoch). Start beim Mittelwert des Blocks, damit kein
   Einschwingen vom ersten Wert aus mitgezählt wird */
static void adc_blk_lowpass_count(adcHist_t *h)
{
    uint16_t k    = s_lp_shift;
    uint32_t half = 1UL << (k - 1u);  /* 0,5 in der Skalierung 2^K, zum Runden */
    uint32_t sum  = 0UL;
    uint32_t y;
    uint16_t i;

    for (i = 0u; i < ADC_BLOCK_LEN; i++) {
        sum += s_block[i];
    }
    h->sum_raw += sum;
    y = ((sum << k) + ADC_BLOCK_LEN / 2UL) / ADC_BLOCK_LEN;

    for (i = 0u; i < ADC_BLOCK_LEN; i++) {
        y = y - ((y + half) >> k) + s_block[i];
        adc_hist_count(h, (uint16_t)((y + half) >> k));
    }
}

bool adcBlockLowpassHistogram(adcHist_t *h)
{
    uint16_t b;

    for (b = 0u; b < ADC_LP_HIST_BLOCKS; b++) {
        if (!adcBlockCapture()) {
            return false;
        }
        if (b == 0u) {
            adc_hist_reset(h, adcBlockMean());
        }
        adc_blk_lowpass_count(h);
    }

    return true;
}

/* ---------- Laufende Filter: schalten ---------- */

bool adcMeanToggle(void)
{
    s_mean_on = !s_mean_on;
    adc_filter_restart_stream();
    return s_mean_on;
}

bool adcLowpassToggle(void)
{
    s_lowpass_on = !s_lowpass_on;
    adc_filter_restart_stream();
    return s_lowpass_on;
}

bool adcMeanGet(void)
{
    return s_mean_on;
}

bool adcLowpassGet(void)
{
    return s_lowpass_on;
}

/* ---------- Histogramm ---------- */

/* Eine Vorlaufumsetzung legt das Fenster fest, danach werden ADC_HIST_SAMPLES
   Einzelumsetzungen per Software ausgezählt, jeweils nach den laufenden
   Filtern (adcmean, adclowpass), falls an. Der Normalbetrieb (TB0.1) ruht
   so lange und läuft danach mit neu gestarteten Filtern wieder */
void adcHistogram(adcHist_t *h)
{
    uint16_t ie = ADC12IE;
    uint16_t i;
    uint16_t n;
    uint16_t x;

    /* Die Interruptroutine darf ADC12IFG0 nicht abholen,
       sonst endet adc_read_single() nie */
    ADC12IE = 0;

    adc_config_single();

    /* Filter starten mit der Vorlaufumsetzung */
    s_filt_hist.started = false;

    /* Vorlaufumsetzung: legt das Fenster fest, wird nicht gezählt */
    n = adc_filter(&s_filt_hist, adc_read_single());
    adc_hist_reset(h, n);

    for (i = 0u; i < ADC_HIST_SAMPLES; i++) {
        x = adc_read_single();
        h->sum_raw += x;
        adc_hist_count(h, adc_filter(&s_filt_hist, x));
    }

    /* Normalbetrieb wiederherstellen, dessen Filter neu starten
       (Interruptroutine ist hier noch gesperrt) */
    s_filt_stream.started = false;
    adc_config_periodic();
    ADC12IE = ie;
}

/* ---------- Interruptroutine ---------- */

#pragma vector = ADC12_VECTOR
__interrupt void adc12_isr(void)
{
    switch (__even_in_range(ADC12IV, 34)) {
    case 6:                           /* ADC12IFG0 */
        s_value     = adc_filter(&s_filt_stream, ADC12MEM0);   /* Lesen löscht ADC12IFG0 */
        s_new_value = true;
        break;
    default:
        break;
    }
}
