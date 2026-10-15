#ifndef SRC_INCLUDE_ADC_H_
#define SRC_INCLUDE_ADC_H_

#include <stdint.h>
#include <stdbool.h>

/* ---------- Konfiguration ---------- */

/* Takt des Timers TB0: ACLK = XT1 (32768 Hz), siehe clk.h */
#define ADC_TIMER_CLK_HZ     32768UL

/* Abtastrate in Hz */
#define ADC_SAMPLE_RATE_HZ   3UL

/* Periodenwert für TB0CCR0 (gerundet): N - 1 mit N = f_Timer / f_A */
#define ADC_TIMER_PERIOD \
    (((ADC_TIMER_CLK_HZ + ADC_SAMPLE_RATE_HZ / 2UL) / ADC_SAMPLE_RATE_HZ) - 1UL)

/* ---------- Referenz ---------- */

/* Interne Referenz für V_R+ (Bitfeld REFVSELx):
   REFVSEL_0 = 1,5 V, REFVSEL_1 = 2,0 V, REFVSEL_2 = 2,5 V */
#define ADC_REFVSEL          REFVSEL_2

/* V_R+ in mV für die Umrechnung, Nennwert passend zu ADC_REFVSEL
   (SLAS590P, Tabelle 8.41). Startwert, mit adcvref_ zur Laufzeit änderbar.
   Ändert nur die Umrechnung, nicht die Referenz selbst (ADC_REFVSEL) */
#define ADC_VREF_MV          2500UL

/* Wartezeit nach Bit REFON in MCLK-Takten.
   t_SETTLE höchstens 75 us (SLAS590P, Tabelle 8.41) -> 400 Takte reichen bis MCLK = 5,3 MHz */
#define ADC_REF_SETTLE_CYCLES 400u

/* Spannung der Signalquelle = Versorgung des Spannungsteilers in mV
   (3V3 des LaunchPads, Nennwert). Geht in den Widerstand ein, weil V_R+ nicht
   die Versorgung des Teilers ist. Startwert, mit adcvsrc_ zur Laufzeit änderbar */
#define ADC_VCC_MV           3300UL

/* ---------- Takt und Abtastzeit (Normalbetrieb und Histogramm) ---------- */

/* Bitfeld ADC12SSELx: Taktquelle ADC12OSC */
#define ADC_SSEL             ADC12SSEL_0

/* Bitfeld ADC12DIVx: Teiler 2. Mit interner Referenz und Bit REFOUT = 0 ist
   f_ADC12CLK bis 2,7 MHz spezifiziert, "ensured when using the ADC12OSC
   divided by 2" (SLAS590P, Tabelle 8.36, Fußnote 3) */
#define ADC_DIV              ADC12DIV_1

/* Bitfeld ADC12SHT0x: Abtastzeit 1024 Takte ADC12CLK */
#define ADC_SHT0             ADC12SHT0_12

/* Festwiderstand R_1 des Spannungsteilers in Ohm.
   Startwert, mit adcr1_ zur Laufzeit änderbar (0,1 ... 65535,0 Ohm) */
#define ADC_R1_OHM           1000UL

/* Nachkommastellen der Mittelwerte (Histogramme): N und U mit ADC_MEAN_DEC,
   R mit ADC_OHM_DEC Stellen. Höchstens 4 bzw. 2: dann bleibt adcFixedToOhm
   auch bei den größten einstellbaren Parametern innerhalb von 64 Bit */
#define ADC_MEAN_DEC         4u
#define ADC_OHM_DEC          2u

/* Nachkommastellen, auf die der Mittelwert N gerundet wird, bevor U, R und T
   daraus berechnet werden. Startwert, mit adcround_ änderbar (0 ... ADC_MEAN_DEC).
   ADC_MEAN_DEC bedeutet keine zusätzliche Rundung */
#define ADC_ROUND_DEC        ADC_MEAN_DEC

/* Offset o des ADC in 0,01 Stufen (Startwert, mit adcoffset_ änderbar).
   Vor jeder Umrechnung in U und R gilt N_korr = N - o (nicht unter 0).
   Die Histogrammbalken und der Mittelwert N bleiben die Rohwerte des ADC */
#define ADC_OFFSET_X100      0

/* Lage von R_T im Spannungsteiler:
   1 -> V_CC - R_1 - Mittelknoten - R_T - GND  (R_T unten)
   0 -> V_CC - R_T - Mittelknoten - R_1 - GND  (R_T oben) */
#define ADC_RT_LOW_SIDE      0

/* ---------- Kalibrierwerte (Befehl adcpreset) ----------
   Ergebnis der Kalibrierung vom 14.10.2026 mit Multimeter und sechs bekannten
   Widerständen (559,2 Ohm ... 67,9 kOhm): Abweichung in R höchstens 0,08 %.
   V_R+ enthält den Verstärkungsfehler des ADC (nur das Verhältnis V_src / V_R+
   ist bestimmt). adcpreset setzt die Laufzeitparameter auf diese Werte */
#define ADC_PRESET_VREF_X100   249371UL /* V_R+ = 2493,71 mV  (adcvref_), in 0,01 mV */
#define ADC_PRESET_VSRC_X100   330600UL /* V_CC = 3306,00 mV  (adcvsrc_), in 0,01 mV */
#define ADC_PRESET_R1_X10      10000UL  /* wirksames R_1 = 1000,0 Ohm  (adcr1_), in 0,1 Ohm */
#define ADC_PRESET_OFFSET_X100 (-56)    /* Offset -0,56 Stufen         (adcoffset_) */
#define ADC_PRESET_ROUND_DEC   4u       /* N ungerundet                (adcround_)  */
#define ADC_PRESET_BLK_RATE    3u       /* 100 kHz                     (adclprate_) */
#define ADC_PRESET_LP_SHIFT    6u       /* tau = 2^6 = 64 Werte        (adclptau_)  */
#define ADC_PRESET_LP_BLOCKS   8u       /* 8 Blöcke = 16384 Werte      (adclpblocks_) */

/* Rückgabewert von adcToOhm, wenn R_T unendlich wäre (Division durch 0) */
#define ADC_OHM_INVALID      0xFFFFFFFFUL

/* Steinhart-Hart-Koeffizienten des NTC 10 kOhm:
     1/T = A + B * ln(R_T) + C * (ln(R_T))^3,  T in K, R_T in Ohm
   Ausgleichsrechnung (kleinste Quadrate in 1/T) über alle 22 Punkte der
   Kennlinie von sensorshop24 (-50 °C ... +150 °C, Stand 2015).
   Abweichung zur Tabelle: <= 0,06 K von -50 °C bis +70 °C,
   bis 0,73 K darüber (dort ist die Tabelle nur auf 0,01 kOhm gerundet) */
#define ADC_SH_A             1.13729162e-03f
#define ADC_SH_B             2.32805103e-04f
#define ADC_SH_C             9.21787251e-08f

/* Rückgabewert von adcToCentiCelsius, wenn R_T ungültig ist */
#define ADC_TEMP_INVALID     ((int32_t)0x80000000L)

/* Zulässiger Temperaturbereich der Messung in 0,001 °C. Liegt die berechnete
   Temperatur außerhalb, liefern adcToCentiCelsius und adcFixedOhmToMilliCelsius
   ADC_TEMP_RANGE, und adclphist / adcstream geben statt der Werte einen Fehler aus */
#define ADC_T_MIN_MC         (-20000L)
#define ADC_T_MAX_MC         110000L
#define ADC_TEMP_RANGE       ((int32_t)0x80000001L)

/* Eigene Kennlinie (Befehl adcntc_): Anzahl der Stützpunkte T/R_T */
#define ADC_NTC_POINTS       3u

/* ---------- Blockmessung mit Tiefpass (Befehle adclprate_, adclphist) ---------- */

/* Anzahl Werte pro Block (2048 * 2 Byte = 4 KB RAM).
   Dauer = ADC_BLOCK_LEN / Rate: 2,05 s (1 kHz), 205 ms (10 kHz), 20,5 ms (100 kHz) */
#define ADC_BLOCK_LEN        2048UL

/* SMCLK in Hz für Timer TB0 während der Blockmessung. main ruft beim Start
   clock_init_xt1() auf -> SMCLK = 2^22 Hz (clk.h). Nach setclock_3
   (4,25 MHz) liegen die Raten um 1,3 % höher */
#define ADC_BLK_SMCLK_HZ     4194304UL

/* Abtastraten der Stufen 1, 2, 3 in Hz */
#define ADC_BLK_RATE1_HZ     1000UL
#define ADC_BLK_RATE2_HZ     10000UL
#define ADC_BLK_RATE3_HZ     100000UL

/* Abtastzeit je Stufe (Bitfeld ADC12SHT0x). Abtastzeit + 13 Takte Umsetzung
   müssen beim kleinsten f_ADC12CLK = 2,1 MHz in eine Periode passen */
#define ADC_BLK_SHT1         ADC12SHT0_12    /* 1024 Takte: 494 us < 1 ms   */
#define ADC_BLK_SHT2         ADC12SHT0_6     /* 128 Takte:  67 us < 100 us  */
#define ADC_BLK_SHT3         ADC12SHT0_0     /* 4 Takte:    8,1 us < 10 us  */

/* Stufe nach dem Start (1 ... 3) */
#define ADC_BLK_RATE_DEFAULT 3u

/* Tiefpass der Blockmessung: y[n] = y[n-1] + (x[n] - y[n-1]) / 2^K,
   Zeitkonstante 2^K Werte, Grenzfrequenz ca. f_A / (2 * pi * 2^K),
   bei K = 6: 1 kHz -> ca. 2,5 Hz, 10 kHz -> ca. 25 Hz, 100 kHz -> ca. 250 Hz.
   ADC_LP_SHIFT ist K nach dem Start, zur Laufzeit mit adclptau_ einstellbar */
#define ADC_LP_SHIFT         6u

/* Grenzen für K. Der Filterzustand wird mit 2^K skaliert in 32 Bit geführt:
   4095 * 2^K < 2^32 -> höchstens 20 */
#define ADC_LP_SHIFT_MIN     1u
#define ADC_LP_SHIFT_MAX     20u

/* Anzahl Blöcke je Histogramm: Startwert, mit adclpblocks_ zur Laufzeit
   änderbar von ADC_LP_BLOCKS_MIN bis ADC_LP_BLOCKS_MAX.
   Höchstens 31 wäre möglich, sonst laufen die 16-Bit-Zähler über */
#define ADC_LP_HIST_BLOCKS   8u
#define ADC_LP_BLOCKS_MIN    1u
#define ADC_LP_BLOCKS_MAX    31u      /* 31 * 2048 = 63488 < 65535 (16-Bit-Zähler) */

/* ---------- Histogramm ---------- */

/* Halbe Fensterbreite in Stufen: das Fenster reicht vom Mittelwert des
   ersten Blocks minus ADC_HIST_HALF bis plus ADC_HIST_HALF */
#define ADC_HIST_HALF        16u

/* Anzahl der Klassen im Fenster */
#define ADC_HIST_BINS        (2u * ADC_HIST_HALF + 1u)

/* ---------- Schnittstelle ---------- */

/* Konfiguriert P6.0, die interne Referenz, ADC12_A und Timer TB0
   und startet die Abtastung */
void adcInit(void);

/* Normalbetrieb (TB0 an ACLK, ADC_SAMPLE_RATE_HZ): liefert true und den
   neuesten Wert N (0 ... 4095), falls seit dem letzten Aufruf ein neuer Wert
   vorliegt, sonst false */
bool adcGet(uint16_t *n);

/* Liefert den zuletzt umgesetzten Wert N (0 ... 4095),
   ohne ihn abzuholen */
uint16_t adcLast(void);

/* Umrechnungsparameter (Befehle adcvref_, adcvsrc_, adcr1_).
   Spannungen in 0,01 mV (1 ... ADC_MV_MAX_X100 = 0,01 ... 6553,50 mV),
   R_1 in 0,1 Ohm (1 ... ADC_R1_MAX_X10 = 0,1 ... 65535,0 Ohm).
   Set-Funktionen liefern false bei 0 bzw. außerhalb des Bereichs,
   Get-Funktionen liefern dieselben Einheiten */
#define ADC_MV_MAX_X100      655350UL
#define ADC_R1_MAX_X10       655350UL
bool     adcVrefSet(uint32_t mv_x100); /* V_R+ in 0,01 mV                      */
bool     adcVsrcSet(uint32_t mv_x100); /* Spannung der Signalquelle in 0,01 mV */
bool     adcR1Set(uint32_t ohm_x10);   /* Festwiderstand R_1 in 0,1 Ohm       */
uint32_t adcVrefGet(void);
uint32_t adcVsrcGet(void);
uint32_t adcR1Get(void);              /* R_1 in 0,1 Ohm                     */

/* Offset o des ADC in 0,01 Stufen (Befehl adcoffset_), mit Vorzeichen */
void     adcOffsetSet(int16_t o_x100);
int16_t  adcOffsetGet(void);

/* Übersetzung N -> Eingangsspannung in mV */
uint16_t adcToMillivolt(uint16_t n);

/* Übersetzung N -> Widerstand R_T in Ohm (ADC_OHM_INVALID bei Division durch 0) */
uint32_t adcToOhm(uint16_t n);

/* Nachkommastellen von N (Befehl adcround_), 0 ... ADC_MEAN_DEC.
   adcRoundSet liefert false außerhalb dieses Bereichs */
bool     adcRoundSet(uint16_t dec);
uint16_t adcRoundGet(void);

/* Mittelwert sum / n als Festkommazahl N * 10^ADC_MEAN_DEC, kaufmännisch
   gerundet auf adcRoundGet() Nachkommastellen, die übrigen Stellen bis
   ADC_MEAN_DEC sind 0. Alle Umrechnungen in U, R und T gehen von diesem
   Wert aus. n = 0 liefert 0. 64 Bit, damit auch die Summe über einen langen
   adcstream passt */
uint32_t adcMeanFixed(uint64_t sum, uint64_t n);

/* Festkomma-N (wie adcMeanFixed) -> Spannung in mV * 10^ADC_MEAN_DEC */
uint64_t adcFixedToMillivolt(uint32_t nq);

/* Festkomma-N (wie adcMeanFixed) -> R_T in Ohm * 10^ADC_OHM_DEC.
   Liefert false, wenn R_T nicht bestimmbar ist (V_in >= V_CC oder N = 0) */
bool adcFixedToOhm(uint32_t nq, uint64_t *rq);

/* Übersetzung N -> Temperatur in 0,01 °C (ADC_TEMP_INVALID bei ungültigem R_T,
   ADC_TEMP_RANGE außerhalb ADC_T_MIN_MC ... ADC_T_MAX_MC) */
int32_t adcToCentiCelsius(uint16_t n);

/* Festkomma-R_T (wie adcFixedToOhm, Ohm * 10^ADC_OHM_DEC) -> Temperatur
   in 0,001 °C (ADC_TEMP_INVALID bei R_T = 0,
   ADC_TEMP_RANGE außerhalb ADC_T_MIN_MC ... ADC_T_MAX_MC) */
int32_t adcFixedOhmToMilliCelsius(uint64_t rq);

/* true, wenn aus dem Festkomma-N (wie adcMeanFixed) ein gültiges R_T und eine
   Temperatur innerhalb ADC_T_MIN_MC ... ADC_T_MAX_MC folgt */
bool adcFixedTempOk(uint32_t nq);

/* Kennlinie aus drei Stützpunkten (Befehl adcntc_).
   adcNtcPointSet speichert Punkt i (0 ... ADC_NTC_POINTS - 1):
   T in 0,01 °C, R_T in 0,01 Ohm. Sind alle drei Punkte gesetzt, werden die
   Steinhart-Hart-Koeffizienten neu berechnet und sofort verwendet.
   Rückgabe:
     ADC_NTC_STORED  Punkt gespeichert, noch nicht alle drei gesetzt
     ADC_NTC_FITTED  Punkt gespeichert, Kennlinie neu berechnet und aktiv
     ADC_NTC_BADARG  i, T oder R_T ungültig, nichts geändert
     ADC_NTC_BADFIT  Punkt gespeichert, aber die drei Punkte ergeben keine
                     fallende Kennlinie (gleiche Werte oder widersprüchlich).
                     Die bisherige Kennlinie bleibt aktiv */
#define ADC_NTC_STORED       0u
#define ADC_NTC_FITTED       1u
#define ADC_NTC_BADARG       2u
#define ADC_NTC_BADFIT       3u
uint16_t adcNtcPointSet(uint16_t i, int32_t t_x100, uint32_t r_x100);

/* Punkt i auslesen. Liefert false, wenn Punkt i noch nicht gesetzt ist */
bool adcNtcPointGet(uint16_t i, int32_t *t_x100, uint32_t *r_x100);

/* true, wenn die Kennlinie aus den eigenen Punkten berechnet ist,
   false bei der Herstellerkennlinie (ADC_SH_A/B/C) */
bool adcNtcFitted(void);

/* Aktive Steinhart-Hart-Koeffizienten */
void adcNtcCoeffGet(float *a, float *b, float *c);

/* Stufe der Blockabtastung: 1 = 1 kHz, 2 = 10 kHz, 3 = 100 kHz.
   adcBlockRateSet liefert false bei ungültiger Stufe,
   adcBlockRateHz liefert die tatsächliche Rate (SMCLK / Periode von TB0) */
bool     adcBlockRateSet(uint16_t step);
uint16_t adcBlockRateGet(void);
uint32_t adcBlockRateHz(void);

/* K des Tiefpasses der Blockmessung (Zeitkonstante 2^K Werte).
   adcLpShiftSet liefert false, wenn K außerhalb ADC_LP_SHIFT_MIN ... MAX liegt */
bool     adcLpShiftSet(uint16_t k);
uint16_t adcLpShiftGet(void);

/* Nimmt ADC_BLOCK_LEN Werte mit der eingestellten Rate per DMA auf
   (blockiert ADC_BLOCK_LEN / Rate), danach läuft wieder der Normalbetrieb
   mit TB0. Liefert false, wenn der Block nicht vollständig aufgenommen wurde */
bool adcBlockCapture(void);

/* Mittelwert des zuletzt aufgenommenen Blocks, Ergebnis N (0 ... 4095) */
uint16_t adcBlockMean(void);

/* Ergebnis von adcBlockLowpassHistogram */
typedef struct {
    uint16_t low;                    /* Ergebnis N, das zu count[0] gehört          */
    uint16_t min;                    /* kleinstes Ergebnis                           */
    uint16_t max;                    /* größtes Ergebnis                             */
    uint16_t below;                  /* Anzahl Ergebnisse unterhalb des Fensters     */
    uint16_t above;                  /* Anzahl Ergebnisse oberhalb des Fensters      */
    uint32_t n;                      /* Anzahl aller gezählten Ergebnisse            */
    uint32_t sum;                    /* Summe aller gezählten Ergebnisse (auch
                                        außerhalb des Fensters) -> Mittelwert       */
    uint32_t sum_raw;                /* Summe der zugehörigen Rohwerte vor dem Filter */
    uint16_t count[ADC_HIST_BINS];   /* count[i] = Anzahl Ergebnisse mit N = low + i */
} adcHist_t;

/* Anzahl Blöcke je Histogramm (Befehl adclpblocks_).
   adcLpBlocksSet liefert false außerhalb ADC_LP_BLOCKS_MIN ... MAX */
bool     adcLpBlocksSet(uint16_t blocks);
uint16_t adcLpBlocksGet(void);

/* Zustand des Blocktiefpasses verwerfen. Der nächste Block startet den
   Tiefpass neu beim Blockmittelwert. adcLpShiftSet und adcBlockRateSet
   rufen das selbst auf (Skalierung bzw. Zeitbasis ändern sich) */
void adcLpReset(void);

/* adcLpBlocksGet() Blöcke mit der eingestellten Rate aufnehmen, mit dem
   Tiefpass (K aus adcLpShiftSet) filtern und die gefilterten Werte zählen.
   Der Tiefpass läuft über die Blockgrenzen und über Aufrufe hinweg weiter,
   neu gestartet wird er nur nach adcLpReset.
   Das Fenster liegt um den Mittelwert des ersten Blocks. Blockiert, gibt nichts
   aus. Liefert false, wenn ein Block nicht vollständig aufgenommen wurde */
bool adcBlockLowpassHistogram(adcHist_t *h);

/* ---------- Werkskalibrierung im TLV ----------
   Adressen aus SLAS590P Tabelle 9-62 (Device Descriptor Table, F5529),
   Bedeutung und Formeln aus SLAU208Q 1.13.5 */
#define ADC_TLV_ADC_TAG_ADDR   0x1A14u   /* ADC12 calibration tag, Wert 11h      */
#define ADC_TLV_ADC_TAG        0x11u
#define ADC_TLV_GAIN_ADDR      0x1A16u   /* CAL_ADC_GAIN_FACTOR = 1/GAIN * 2^15  */
#define ADC_TLV_OFFSET_ADDR    0x1A18u   /* CAL_ADC_OFFSET, Zweierkomplement     */
#define ADC_TLV_T30_15_ADDR    0x1A1Au   /* Temperatursensor 30 °C, 1,5-V-Ref.   */
#define ADC_TLV_T85_15_ADDR    0x1A1Cu   /* Temperatursensor 85 °C, 1,5-V-Ref.   */
#define ADC_TLV_T30_20_ADDR    0x1A1Eu
#define ADC_TLV_T85_20_ADDR    0x1A20u
#define ADC_TLV_T30_25_ADDR    0x1A22u
#define ADC_TLV_T85_25_ADDR    0x1A24u
#define ADC_TLV_REF_TAG_ADDR   0x1A26u   /* REF calibration tag, Wert 12h        */
#define ADC_TLV_REF_TAG        0x12u
#define ADC_TLV_REF15_ADDR     0x1A28u   /* CAL_ADC_15VREF_FACTOR = V_REF/1,5 V * 2^15 */
#define ADC_TLV_REF20_ADDR     0x1A2Au   /* CAL_ADC_20VREF_FACTOR */
#define ADC_TLV_REF25_ADDR     0x1A2Cu   /* CAL_ADC_25VREF_FACTOR */

/* Inhalt der Werkskalibrierung. Index 0/1/2 = Referenz 1,5/2,0/2,5 V */
typedef struct {
    bool     adc_valid;              /* Tag an 01A14h stimmt (11h)          */
    bool     ref_valid;              /* Tag an 01A26h stimmt (12h)          */
    uint16_t gain;                   /* CAL_ADC_GAIN_FACTOR                 */
    int16_t  offset;                 /* CAL_ADC_OFFSET in Stufen            */
    uint16_t t30[3];                 /* Temperatursensor bei 30 °C, N       */
    uint16_t t85[3];                 /* Temperatursensor bei 85 °C, N       */
    uint16_t ref[3];                 /* CAL_ADC_xxVREF_FACTOR               */
} adcTlv_t;

/* Alle Werte der Werkskalibrierung aus dem TLV lesen */
void adcTlvRead(adcTlv_t *t);

/* Werkskalibrierung in die Umrechnung übernehmen (adcvref_, adcoffset_):
   V_R+ = Nennspannung der eingestellten Referenz (ADC_REFVSEL) * Referenzfaktor / 2^15,
   bei use_gain zusätzlich * CAL_ADC_GAIN_FACTOR / 2^15.
   Offset: SLAU208Q Gl. 7 addiert CAL_ADC_OFFSET nach der Verstärkungskorrektur,
   adcoffset_ wird vor der Skalierung abgezogen -> o = -CAL_ADC_OFFSET / Faktor.
   Liefert false (und ändert nichts), wenn die Tags nicht stimmen */
bool adcTlvApply(bool use_gain);

/* AV_CC über Kanal 11 messen: (AV_CC - AV_SS) / 2 über den internen Teiler
   (SLAS590P 8.39: Teilerverhältnis 0,48 / 0,5 / 0,52, nicht werkskalibriert).
   Abtastung 100 kHz (Stufe 3, Abtastzeit 4 Takte = 1,48 ... 1,90 us
   >= t_VMID(sample) 1000 ns), Tiefpass mit tau = 2^ADC_VCC_LP_SHIFT Werten.
   ADC_VCC_BLOCKS Blöcke zu je ADC_BLOCK_LEN Werten:
   10 * 2048 = 20480 Werte = 5 * tau = ca. 205 ms = ca. 10,25 Perioden 50 Hz.
   Eingestellte Rate und Kanal A0 werden danach wiederhergestellt */
#define ADC_VCC_LP_SHIFT       12u
#define ADC_VCC_BLOCKS         10u

/* Mittelwert der gefilterten Werte von Kanal 11 als Festkomma-N
   (N * 10^ADC_MEAN_DEC, wie adcMeanFixed). Liefert false bei Timeout */
bool adcVccMeasure(uint32_t *nq);

#endif /* SRC_INCLUDE_ADC_H_ */
