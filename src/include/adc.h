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
   Startwert, mit adcr1_ zur Laufzeit änderbar (1 ... 65535 Ohm) */
#define ADC_R1_OHM           1000UL

/* Nachkommastellen der Mittelwerte (Histogramme): N und U mit ADC_MEAN_DEC,
   R mit ADC_OHM_DEC Stellen. Höchstens 4 bzw. 2: dann bleibt adcFixedToOhm
   auch bei den größten einstellbaren Parametern innerhalb von 64 Bit */
#define ADC_MEAN_DEC         4u
#define ADC_OHM_DEC          2u

/* Lage von R_T im Spannungsteiler:
   1 -> V_CC - R_1 - Mittelknoten - R_T - GND  (R_T unten)
   0 -> V_CC - R_T - Mittelknoten - R_1 - GND  (R_T oben) */
#define ADC_RT_LOW_SIDE      0

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

/* Grenzen für K. Höchstens 9: der Start rechnet (Blocksumme << K), und
   2048 * 4095 * 2^9 passt gerade noch in 32 Bit */
#define ADC_LP_SHIFT_MIN     1u
#define ADC_LP_SHIFT_MAX     9u

/* Anzahl Blöcke je Histogramm (höchstens 31, sonst laufen die 16-Bit-Zähler über) */
#define ADC_LP_HIST_BLOCKS   8u

/* Anzahl gezählter Werte je Histogramm */
#define ADC_LP_HIST_SAMPLES  (ADC_LP_HIST_BLOCKS * ADC_BLOCK_LEN)

/* ---------- Histogramm ---------- */

/* Anzahl der Umsetzungen, die ausgezählt werden (höchstens 65535) */
#define ADC_HIST_SAMPLES     1024u

/* Halbe Fensterbreite in Stufen: das Fenster reicht vom Ergebnis der
   Vorlaufumsetzung minus ADC_HIST_HALF bis plus ADC_HIST_HALF */
#define ADC_HIST_HALF        16u

/* Anzahl der Klassen im Fenster */
#define ADC_HIST_BINS        (2u * ADC_HIST_HALF + 1u)

/* ---------- Laufende Filter (Befehle adcmean, adclowpass) ---------- */

/* Gleitender Mittelwert über die letzten ADC_FILT_MEAN_LEN Werte */
#define ADC_FILT_MEAN_LEN    16u

/* Tiefpass erster Ordnung: y[n] = y[n-1] + (x[n] - y[n-1]) / 2^K,
   K = ADC_FILT_LP_SHIFT (mindestens 1). Zeitkonstante ca. 2^K Werte */
#define ADC_FILT_LP_SHIFT    4u

/* ---------- Schnittstelle ---------- */

/* Konfiguriert P6.0, die interne Referenz, ADC12_A und Timer TB0
   und startet die Abtastung */
void adcInit(void);

/* Liefert true und den neuesten Wert N (0 ... 4095, gefiltert, falls
   adcmean/adclowpass an), falls seit dem letzten Aufruf ein neuer Wert
   vorliegt, sonst false */
bool adcGet(uint16_t *n);

/* Liefert den zuletzt umgesetzten Wert N (0 ... 4095, gefiltert wie adcGet),
   ohne ihn abzuholen */
uint16_t adcLast(void);

/* Umrechnungsparameter (Befehle adcvref_, adcvsrc_, adcr1_).
   Spannungen in 0,1 mV (1 ... 65535 = 0,1 ... 6553,5 mV), R_1 in Ohm.
   Set-Funktionen liefern false bei 0, Get-Funktionen liefern dieselben Einheiten */
bool     adcVrefSet(uint16_t mv_x10); /* V_R+ in 0,1 mV                      */
bool     adcVsrcSet(uint16_t mv_x10); /* Spannung der Signalquelle in 0,1 mV */
bool     adcR1Set(uint16_t ohm);      /* Festwiderstand R_1 in Ohm          */
uint16_t adcVrefGet(void);
uint16_t adcVsrcGet(void);
uint16_t adcR1Get(void);

/* Übersetzung N -> Eingangsspannung in mV */
uint16_t adcToMillivolt(uint16_t n);

/* Übersetzung N -> Widerstand R_T in Ohm (ADC_OHM_INVALID bei Division durch 0) */
uint32_t adcToOhm(uint16_t n);

/* Mittelwert sum / n als Festkommazahl mit ADC_MEAN_DEC Nachkommastellen
   (N * 10^ADC_MEAN_DEC), kaufmännisch gerundet. n = 0 liefert 0 */
uint32_t adcMeanFixed(uint32_t sum, uint32_t n);

/* Festkomma-N (wie adcMeanFixed) -> Spannung in mV * 10^ADC_MEAN_DEC */
uint64_t adcFixedToMillivolt(uint32_t nq);

/* Festkomma-N (wie adcMeanFixed) -> R_T in Ohm * 10^ADC_OHM_DEC.
   Liefert false, wenn R_T nicht bestimmbar ist (V_in >= V_CC oder N = 0) */
bool adcFixedToOhm(uint32_t nq, uint64_t *rq);

/* Übersetzung N -> Temperatur in 0,01 °C (ADC_TEMP_INVALID bei ungültigem R_T) */
int32_t adcToCentiCelsius(uint16_t n);

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

/* Laufende Filter für Normalbetrieb (adcGet, adcLast) und Histogramm.
   Reihenfolge: Rohwert -> gleitender Mittelwert (falls an) -> Tiefpass (falls an).
   Umschalten startet die Filter des Normalbetriebs neu.
   Toggle-Funktionen liefern den neuen Zustand */
bool adcMeanToggle(void);
bool adcLowpassToggle(void);
bool adcMeanGet(void);
bool adcLowpassGet(void);

/* Ergebnis von adcHistogram */
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

/* Eine Vorlaufumsetzung legt das Fenster fest, danach werden ADC_HIST_SAMPLES
   Umsetzungen ausgezählt. Blockiert bis zur letzten Umsetzung, gibt nichts aus.
   Danach läuft wieder der Normalbetrieb mit TB0 */
void adcHistogram(adcHist_t *h);

/* ADC_LP_HIST_BLOCKS Blöcke mit der eingestellten Rate aufnehmen, jeden Block
   mit dem Tiefpass (K aus adcLpShiftSet) filtern und die gefilterten Werte zählen.
   Das Fenster liegt um den Mittelwert des ersten Blocks. Blockiert, gibt nichts
   aus. Liefert false, wenn ein Block nicht vollständig aufgenommen wurde */
bool adcBlockLowpassHistogram(adcHist_t *h);

#endif /* SRC_INCLUDE_ADC_H_ */
