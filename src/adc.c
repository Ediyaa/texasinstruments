#include <msp430.h>
#include <math.h>
#include "include/adc.h"

/* ---------- interner Zustand ---------- */

static volatile uint16_t s_value;      /* letzter Wert aus ADC12MEM0 */

/* Umrechnungsparameter, Startwerte aus adc.h, mit adcvref_, adcvsrc_, adcr1_ änderbar */
static uint32_t s_vref_x100 = ADC_VREF_MV * 100UL;   /* V_R+ in 0,01 mV                      */
static uint32_t s_vsrc_x100 = ADC_VCC_MV * 100UL;    /* Spannung der Signalquelle in 0,01 mV */
static uint32_t s_r1_x10  = ADC_R1_OHM * 10UL;       /* Festwiderstand R_1 in 0,1 Ohm   */
static int16_t  s_offset_x100 = (int16_t)ADC_OFFSET_X100;   /* Offset o in 0,01 Stufen */
static uint16_t s_round_dec = ADC_ROUND_DEC;         /* Nachkommastellen von N (adcround_) */

/* Aktive Steinhart-Hart-Koeffizienten (Start: Herstellerkennlinie) */
static float s_sh_a = ADC_SH_A;
static float s_sh_b = ADC_SH_B;
static float s_sh_c = ADC_SH_C;
static bool  s_sh_fitted = false;

/* Stützpunkte für adcntc_: T in 0,01 °C, R_T in 0,01 Ohm */
static int32_t  s_ntc_t[ADC_NTC_POINTS];
static uint32_t s_ntc_r[ADC_NTC_POINTS];
static bool     s_ntc_set[ADC_NTC_POINTS];

static uint64_t adc_pow10(uint16_t d);               /* weiter unten definiert */
static volatile bool     s_new_value;  /* true, wenn noch nicht abgeholt */

/* Messblock für adcBlockCapture. NOINIT: die 4 KB werden beim Start nicht genullt */
#pragma NOINIT(s_block)
static uint16_t s_block[ADC_BLOCK_LEN];

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

/* V_in = (N - o) * V_R+ / 4095 (SLAU208Q, 28.2.1), kaufmännisch gerundet auf mV.
   Rechnet über adcFixedToMillivolt, damit der Offset o gleich behandelt wird */
uint16_t adcToMillivolt(uint16_t n)
{
    uint64_t scale = adc_pow10(ADC_MEAN_DEC);
    uint64_t u     = adcFixedToMillivolt((uint32_t)((uint64_t)n * scale));

    return (uint16_t)((u + scale / 2ULL) / scale);
}

/* R_T aus N, kaufmännisch gerundet auf Ohm. Rechnet über adcFixedToOhm
   (Offset o, Formeln und Wertebereich siehe dort). ADC_OHM_INVALID, wenn R_T
   nicht bestimmbar ist oder nicht in 32 Bit passt */
uint32_t adcToOhm(uint16_t n)
{
    uint64_t scale = adc_pow10(ADC_MEAN_DEC);
    uint64_t rs    = adc_pow10(ADC_OHM_DEC);
    uint64_t rq;
    uint64_t r;

    if (!adcFixedToOhm((uint32_t)((uint64_t)n * scale), &rq)) {
        return ADC_OHM_INVALID;
    }

    r = (rq + rs / 2ULL) / rs;
    return (r >= (uint64_t)ADC_OHM_INVALID) ? ADC_OHM_INVALID : (uint32_t)r;
}

/* ---------- Umrechnungsparameter ---------- */

bool adcVrefSet(uint32_t mv_x100)
{
    if (mv_x100 == 0UL || mv_x100 > ADC_MV_MAX_X100) {
        return false;
    }
    s_vref_x100 = mv_x100;
    return true;
}

bool adcVsrcSet(uint32_t mv_x100)
{
    if (mv_x100 == 0UL || mv_x100 > ADC_MV_MAX_X100) {
        return false;
    }
    s_vsrc_x100 = mv_x100;
    return true;
}

bool adcR1Set(uint32_t ohm_x10)
{
    if (ohm_x10 == 0UL || ohm_x10 > ADC_R1_MAX_X10) {
        return false;
    }
    s_r1_x10 = ohm_x10;
    return true;
}

uint32_t adcVrefGet(void)
{
    return s_vref_x100;
}

uint32_t adcVsrcGet(void)
{
    return s_vsrc_x100;
}

uint32_t adcR1Get(void)
{
    return s_r1_x10;
}

void adcOffsetSet(int16_t o_x100)
{
    s_offset_x100 = o_x100;
}

int16_t adcOffsetGet(void)
{
    return s_offset_x100;
}

/* ---------- Umrechnung mit Nachkommastellen (Mittelwerte) ---------- */

/* 10^d */
static uint64_t adc_pow10(uint16_t d)
{
    uint64_t p = 1ULL;

    while (d-- > 0u) {
        p *= 10ULL;
    }
    return p;
}

bool adcRoundSet(uint16_t dec)
{
    if (dec > ADC_MEAN_DEC) {
        return false;
    }
    s_round_dec = dec;
    return true;
}

uint16_t adcRoundGet(void)
{
    return s_round_dec;
}

/* Direkt aus sum / n auf s_round_dec Stellen runden und danach auf
   ADC_MEAN_DEC Stellen hochskalieren. Nicht erst auf ADC_MEAN_DEC Stellen
   runden und dann noch einmal: doppeltes Runden kann eine Stelle falsch
   aufrunden (480,44996 -> 480,4500 -> 480,5 statt 480,4) */
uint32_t adcMeanFixed(uint64_t sum, uint64_t n)
{
    uint64_t q;

    if (n == 0ULL) {
        return 0UL;
    }
    q = (sum * adc_pow10(s_round_dec) + n / 2ULL) / n;
    return (uint32_t)(q * adc_pow10(ADC_MEAN_DEC - s_round_dec));
}

/* Festkomma-N um den Offset korrigieren: N - o, nicht unter 0.
   o liegt in 0,01 Stufen vor, N in 10^ADC_MEAN_DEC -> o mit 10^(ADC_MEAN_DEC - 2)
   hochskalieren (ADC_MEAN_DEC >= 2) */
static uint32_t adc_fixed_corr(uint32_t nq)
{
    int64_t v = (int64_t)nq
              - (int64_t)s_offset_x100 * (int64_t)adc_pow10(ADC_MEAN_DEC - 2u);

    return (v > 0) ? (uint32_t)v : 0UL;
}

/* U = (N - o) * V_R+ / 4095, N und U jeweils mit 10^ADC_MEAN_DEC skaliert,
   V_R+ in 0,01 mV -> durch 4095 * 100 teilen */
uint64_t adcFixedToMillivolt(uint32_t nq)
{
    nq = adc_fixed_corr(nq);
    return ((uint64_t)nq * s_vref_x100 + 409500ULL / 2ULL) / 409500ULL;
}

/* R_T aus (N - o), N mit 10^ADC_MEAN_DEC skaliert, Ergebnis in Ohm * 10^ADC_OHM_DEC.
   R_T oben:  R_T = R_1 * (4095 * V_CC - N * V_R+) / (N * V_R+)
   R_T unten: R_T = R_1 * N * V_R+ / (4095 * V_CC - N * V_R+)
   V_CC, V_R+ in 0,01 mV (die Einheit kürzt sich), R_1 in 0,1 Ohm -> mit
   10^(ADC_OHM_DEC - 1) statt 10^ADC_OHM_DEC multiplizieren (ADC_OHM_DEC >= 1).
   u_ges höchstens 4095 * 655350 * 10^4 = 2,68e13. R_1 * u_ges würde 2^64
   überschreiten, deshalb in zwei Schritten: erst z = Spannungsanteil * 10^(D-1)
   durch den Nenner teilen, dann Quotient und Rest einzeln mit R_1 multiplizieren.
   Größtes Produkt R_1 * Rest < 655350 * 2,68e13 = 1,76e19 < 2^64 = 1,84e19 */
bool adcFixedToOhm(uint32_t nq, uint64_t *rq)
{
    uint64_t u_in  = (uint64_t)adc_fixed_corr(nq) * s_vref_x100;        /* 4095 * V_in * 10^D, 0,01 mV */
    uint64_t u_ges = 4095ULL * s_vsrc_x100 * adc_pow10(ADC_MEAN_DEC);    /* 4095 * V_CC * 10^D, 0,01 mV */
    uint64_t z;
    uint64_t nenner;
    uint64_t q;
    uint64_t r;

    if (u_in >= u_ges) {
        return false;                                                  /* V_in >= V_CC */
    }

#if ADC_RT_LOW_SIDE
    z      = u_in * adc_pow10(ADC_OHM_DEC - 1u);
    nenner = u_ges - u_in;
#else
    z      = (u_ges - u_in) * adc_pow10(ADC_OHM_DEC - 1u);
    nenner = u_in;
#endif

    if (nenner == 0ULL) {
        return false;
    }

    q = z / nenner;
    r = z % nenner;

    if (q > (0xFFFFFFFFFFFFFFFFULL - 0xFFFFFFFFULL) / s_r1_x10) {
        return false;                                                  /* Ergebnis zu groß */
    }

    *rq = s_r1_x10 * q + (s_r1_x10 * r + nenner / 2ULL) / nenner;
    return true;
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
    t = 1.0f / (s_sh_a + s_sh_b * l + s_sh_c * l * l * l);         /* in K */
    t = (t - 273.15f) * 100.0f;                                      /* in 0,01 °C */

    if (!(t >= (float)ADC_T_MIN_MC / 10.0f && t <= (float)ADC_T_MAX_MC / 10.0f)) {
        return ADC_TEMP_RANGE;        /* auch bei NaN */
    }
    return (int32_t)((t >= 0.0f) ? (t + 0.5f) : (t - 0.5f));
}

/* Steinhart-Hart wie oben, aber aus R_T in Ohm * 10^ADC_OHM_DEC,
   Ergebnis in 0,001 °C, kaufmännisch gerundet */
int32_t adcFixedOhmToMilliCelsius(uint64_t rq)
{
    float l;
    float t;

    if (rq == 0ULL) {
        return ADC_TEMP_INVALID;
    }

    l = logf((float)rq / (float)adc_pow10(ADC_OHM_DEC));
    t = 1.0f / (s_sh_a + s_sh_b * l + s_sh_c * l * l * l);         /* in K */
    t = (t - 273.15f) * 1000.0f;                                     /* in 0,001 °C */

    if (!(t >= (float)ADC_T_MIN_MC && t <= (float)ADC_T_MAX_MC)) {
        return ADC_TEMP_RANGE;        /* auch bei NaN */
    }
    return (int32_t)((t >= 0.0f) ? (t + 0.5f) : (t - 0.5f));
}

bool adcFixedTempOk(uint32_t nq)
{
    uint64_t rq;
    int32_t  t;

    if (!adcFixedToOhm(nq, &rq)) {
        return false;                 /* V_in >= V_CC: R_T nicht bestimmbar */
    }
    t = adcFixedOhmToMilliCelsius(rq);
    return (t != ADC_TEMP_INVALID && t != ADC_TEMP_RANGE);
}

/* Koeffizienten aus den drei Stützpunkten berechnen (Gleichungssystem
     1/T_i = A + B * L_i + C * L_i^3,  L_i = ln(R_T,i),  i = 1, 2, 3
   exakt gelöst). Rechnung in double, weil C aus einer kleinen Differenz
   entsteht. Liefert false, wenn das System nicht lösbar ist oder die
   Kennlinie zwischen den Punkten nicht fallend wäre */
static bool adc_ntc_fit(void)
{
    double l[ADC_NTC_POINTS];
    double y[ADC_NTC_POINTS];
    double g2, g3, a, b, cc, lmin, lmax, d;
    uint16_t i;

    for (i = 0u; i < ADC_NTC_POINTS; i++) {
        l[i] = log((double)s_ntc_r[i] / 100.0);
        y[i] = 1.0 / ((double)s_ntc_t[i] / 100.0 + 273.15);
    }

    if (l[1] == l[0] || l[2] == l[0] || l[2] == l[1]) {
        return false;
    }

    g2 = (y[1] - y[0]) / (l[1] - l[0]);
    g3 = (y[2] - y[0]) / (l[2] - l[0]);
    d  = l[0] + l[1] + l[2];
    if (d == 0.0) {
        return false;
    }
    cc = (g3 - g2) / (l[2] - l[1]) / d;
    b  = g2 - cc * (l[0] * l[0] + l[0] * l[1] + l[1] * l[1]);
    a  = y[0] - (b + l[0] * l[0] * cc) * l[0];

    /* NTC: 1/T muss mit ln(R_T) steigen, d(1/T)/dL = B + 3 C L^2 > 0.
       Der Ausdruck ist in L^2 linear, also genügt die Prüfung an den Enden */
    lmin = l[0];
    lmax = l[0];
    for (i = 1u; i < ADC_NTC_POINTS; i++) {
        if (l[i] < lmin) lmin = l[i];
        if (l[i] > lmax) lmax = l[i];
    }
    if (b + 3.0 * cc * lmin * lmin <= 0.0 || b + 3.0 * cc * lmax * lmax <= 0.0) {
        return false;
    }

    s_sh_a = (float)a;
    s_sh_b = (float)b;
    s_sh_c = (float)cc;
    s_sh_fitted = true;
    return true;
}

uint16_t adcNtcPointSet(uint16_t i, int32_t t_x100, uint32_t r_x100)
{
    uint16_t k;

    if (i >= ADC_NTC_POINTS || t_x100 <= -27315L || r_x100 == 0UL) {
        return ADC_NTC_BADARG;
    }

    s_ntc_t[i]   = t_x100;
    s_ntc_r[i]   = r_x100;
    s_ntc_set[i] = true;

    for (k = 0u; k < ADC_NTC_POINTS; k++) {
        if (!s_ntc_set[k]) {
            return ADC_NTC_STORED;
        }
    }

    return adc_ntc_fit() ? ADC_NTC_FITTED : ADC_NTC_BADFIT;
}

bool adcNtcPointGet(uint16_t i, int32_t *t_x100, uint32_t *r_x100)
{
    if (i >= ADC_NTC_POINTS || !s_ntc_set[i]) {
        return false;
    }
    *t_x100 = s_ntc_t[i];
    *r_x100 = s_ntc_r[i];
    return true;
}

bool adcNtcFitted(void)
{
    return s_sh_fitted;
}

void adcNtcCoeffGet(float *a, float *b, float *cc)
{
    *a  = s_sh_a;
    *b  = s_sh_b;
    *cc = s_sh_c;
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
static uint16_t s_blk_inch = ADC12INCH_0;            /* Kanal der Blockmessung     */
static uint16_t s_lp_shift = ADC_LP_SHIFT;           /* K des Blocktiefpasses      */
static uint16_t s_lp_blocks = ADC_LP_HIST_BLOCKS;    /* Blöcke je Histogramm       */
static uint32_t s_lp_y;                              /* Tiefpasszustand y * 2^K    */
static bool     s_lp_valid = false;                  /* false: Neustart beim Block */

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
    adcLpReset();                     /* Zeitbasis geändert */
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
    adcLpReset();                     /* Skalierung 2^K geändert */
    return true;
}

void adcLpReset(void)
{
    s_lp_valid = false;
}

uint16_t adcLpShiftGet(void)
{
    return s_lp_shift;
}

bool adcLpBlocksSet(uint16_t blocks)
{
    if (blocks < ADC_LP_BLOCKS_MIN || blocks > ADC_LP_BLOCKS_MAX) {
        return false;
    }
    s_lp_blocks = blocks;
    return true;
}

uint16_t adcLpBlocksGet(void)
{
    return s_lp_blocks;
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
    ADC12MCTL0 = ADC12SREF_1 | s_blk_inch;    /* A0, bei adcVccMeasure Kanal 11 */

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

    /* Normalbetrieb wiederherstellen */
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
   y wird mit 2^K skaliert in s_lp_y geführt, y / 2^K gerundet (abgeschnitten
   läge der Ausgang im Mittel zu hoch). s_lp_y läuft über Blöcke und Aufrufe
   weiter. Nur nach adcLpReset startet y beim Mittelwert des Blocks, damit kein
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

    if (s_lp_valid) {
        y = s_lp_y;
    } else {
        /* Start = Blockmittel * 2^K, gerundet. Erst ganzzahligen Teil und Rest
           trennen, sonst läuft (sum << K) bei großem K über 32 Bit */
        uint32_t q = sum / ADC_BLOCK_LEN;
        uint32_t r = sum % ADC_BLOCK_LEN;

        y = (q << k) + ((r << k) + ADC_BLOCK_LEN / 2UL) / ADC_BLOCK_LEN;
        s_lp_valid = true;
    }

    for (i = 0u; i < ADC_BLOCK_LEN; i++) {
        y = y - ((y + half) >> k) + s_block[i];
        adc_hist_count(h, (uint16_t)((y + half) >> k));
    }

    s_lp_y = y;
}

bool adcBlockLowpassHistogram(adcHist_t *h)
{
    uint16_t b;

    for (b = 0u; b < s_lp_blocks; b++) {
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

/* ---------- Werkskalibrierung im TLV ---------- */

#define ADC_TLV8(a)   (*(const volatile uint8_t  *)(a))
#define ADC_TLV16(a)  (*(const volatile uint16_t *)(a))

void adcTlvRead(adcTlv_t *t)
{
    t->adc_valid = (ADC_TLV8(ADC_TLV_ADC_TAG_ADDR) == ADC_TLV_ADC_TAG);
    t->ref_valid = (ADC_TLV8(ADC_TLV_REF_TAG_ADDR) == ADC_TLV_REF_TAG);

    t->gain   = ADC_TLV16(ADC_TLV_GAIN_ADDR);
    t->offset = (int16_t)ADC_TLV16(ADC_TLV_OFFSET_ADDR);

    t->t30[0] = ADC_TLV16(ADC_TLV_T30_15_ADDR);
    t->t85[0] = ADC_TLV16(ADC_TLV_T85_15_ADDR);
    t->t30[1] = ADC_TLV16(ADC_TLV_T30_20_ADDR);
    t->t85[1] = ADC_TLV16(ADC_TLV_T85_20_ADDR);
    t->t30[2] = ADC_TLV16(ADC_TLV_T30_25_ADDR);
    t->t85[2] = ADC_TLV16(ADC_TLV_T85_25_ADDR);

    t->ref[0] = ADC_TLV16(ADC_TLV_REF15_ADDR);
    t->ref[1] = ADC_TLV16(ADC_TLV_REF20_ADDR);
    t->ref[2] = ADC_TLV16(ADC_TLV_REF25_ADDR);
}

bool adcTlvApply(bool use_gain)
{
    adcTlv_t t;
    uint16_t idx;
    uint32_t nom_x100;                /* Nennspannung der Referenz in 0,01 mV */
    uint64_t f;                       /* Gesamtfaktor * 2^shift             */
    uint16_t shift;
    uint64_t vref_x100;
    int64_t  o_x100;

    adcTlvRead(&t);
    if (!t.ref_valid || !t.adc_valid) {
        return false;                 /* Offset und Verstärkung aus dem ADC12-Block */
    }

    if (ADC_REFVSEL == REFVSEL_0) {
        idx = 0u; nom_x100 = 150000UL;
    } else if (ADC_REFVSEL == REFVSEL_1) {
        idx = 1u; nom_x100 = 200000UL;
    } else {
        idx = 2u; nom_x100 = 250000UL;
    }

    if (t.ref[idx] == 0u || t.ref[idx] == 0xFFFFu || (use_gain && (t.gain == 0u || t.gain == 0xFFFFu))) {
        return false;                 /* leer oder gelöscht */
    }

    f     = t.ref[idx];
    shift = 15u;
    if (use_gain) {
        f     *= t.gain;
        shift  = 30u;
    }

    /* V_R+ = Nenn * f / 2^shift, gerundet */
    vref_x100 = ((uint64_t)nom_x100 * f + (1ULL << (shift - 1u))) >> shift;
    if (vref_x100 == 0ULL || vref_x100 > ADC_MV_MAX_X100) {
        return false;
    }

    /* o = -CAL_ADC_OFFSET / (f / 2^shift), in 0,01 Stufen, gerundet */
    o_x100 = -((int64_t)t.offset * 100LL * (int64_t)(1ULL << shift));
    o_x100 = (o_x100 >= 0) ? (o_x100 + (int64_t)(f / 2ULL)) / (int64_t)f
                           : (o_x100 - (int64_t)(f / 2ULL)) / (int64_t)f;
    if (o_x100 > 32767LL || o_x100 < -32768LL) {
        return false;
    }

    adcVrefSet((uint32_t)vref_x100);
    adcOffsetSet((int16_t)o_x100);
    return true;
}

/* ---------- AV_CC über Kanal 11 ---------- */

bool adcVccMeasure(uint32_t *nq)
{
    const uint16_t k    = ADC_VCC_LP_SHIFT;
    const uint32_t half = 1UL << (k - 1u);
    uint16_t step = s_blk_step;
    uint16_t b;
    uint16_t i;
    uint32_t y = 0UL;                 /* Tiefpasszustand * 2^k            */
    uint64_t total = 0ULL;            /* Summe der Zustände (je * 2^k)    */
    uint32_t n = 0UL;
    bool     ok = true;

    s_blk_step = 3u;                  /* 100 kHz */
    s_blk_inch = ADC12INCH_11;        /* (AV_CC - AV_SS) / 2 */

    for (b = 0u; b < ADC_VCC_BLOCKS; b++) {
        if (!adcBlockCapture()) {
            ok = false;
            break;
        }
        if (b == 0u) {
            /* Start beim Blockmittel * 2^k, ohne Überlauf (siehe adc_blk_lowpass_count) */
            uint32_t sum = 0UL;
            uint32_t q;
            uint32_t r;

            for (i = 0u; i < ADC_BLOCK_LEN; i++) {
                sum += s_block[i];
            }
            q = sum / ADC_BLOCK_LEN;
            r = sum % ADC_BLOCK_LEN;
            y = (q << k) + ((r << k) + ADC_BLOCK_LEN / 2UL) / ADC_BLOCK_LEN;
        }
        for (i = 0u; i < ADC_BLOCK_LEN; i++) {
            y = y - ((y + half) >> k) + s_block[i];
            total += y;
        }
        n += ADC_BLOCK_LEN;
    }

    s_blk_inch = ADC12INCH_0;
    s_blk_step = step;

    if (!ok) {
        return false;
    }

    /* Mittel der Zustände / 2^k = Mittel der gefilterten Werte, ohne Rundung je Wert */
    *nq = adcMeanFixed(total, (uint64_t)n << k);
    return true;
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
