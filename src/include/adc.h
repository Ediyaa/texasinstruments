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

/* V_R+ in mV, Nennwert passend zu ADC_REFVSEL (SLAS590P, Tabelle 8.41) */
#define ADC_VREF_MV          2500UL

/* Wartezeit nach Bit REFON in MCLK-Takten.
   t_SETTLE höchstens 75 us (SLAS590P, Tabelle 8.41) -> 400 Takte reichen bis MCLK = 5,3 MHz */
#define ADC_REF_SETTLE_CYCLES 400u

/* Versorgung des Spannungsteilers in mV (3V3 des LaunchPads, Nennwert).
   Geht in den Widerstand ein, weil V_R+ nicht die Versorgung des Teilers ist */
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

/* Festwiderstand R_1 des Spannungsteilers in Ohm */
#define ADC_R1_OHM           1000UL

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

/* Blockmessung: Anzahl Werte pro Block (2048 * 2 Byte = 4 KB RAM)
   Dauer bei f_A ca. 48 ... 61 kHz: ca. 34 ... 43 ms */
#define ADC_BLOCK_LEN        2048UL

/* Abbruch der Blockmessung nach so vielen Durchläufen der Warteschleife
   (mindestens ca. 8 MCLK-Takte je Durchlauf: bei 4,25 MHz >= ca. 190 ms,
   bei 1 MHz ca. 1 s). Verhindert, dass die Konsole hängen bleibt */
#define ADC_BLOCK_TIMEOUT    100000UL

/* Tiefpass: y[n] = y[n-1] + (x[n] - y[n-1]) / 2^K
   Grenzfrequenz ca. f_A / (2 * pi * 2^K), bei f_A ca. 55 kHz:
   K = 4 -> ca. 550 Hz, K = 6 -> ca. 140 Hz, K = 8 -> ca. 35 Hz
   Einschwingen: Zeitkonstante 2^K Werte, K <= 8 ist bei 2048 Werten eingeschwungen */
#define ADC_LP_SHIFT         6u

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

/* Übersetzung N -> Eingangsspannung in mV */
uint16_t adcToMillivolt(uint16_t n);

/* Übersetzung N -> Widerstand R_T in Ohm (ADC_OHM_INVALID bei Division durch 0) */
uint32_t adcToOhm(uint16_t n);

/* Übersetzung N -> Temperatur in 0,01 °C (ADC_TEMP_INVALID bei ungültigem R_T) */
int32_t adcToCentiCelsius(uint16_t n);

/* Nimmt ADC_BLOCK_LEN Werte per DMA auf (blockiert ca. 34 ... 43 ms),
   danach läuft wieder der Normalbetrieb mit TB0.
   Liefert false, wenn der Block nicht vollständig aufgenommen wurde */
bool adcBlockCapture(void);

/* Auswertung des zuletzt aufgenommenen Blocks, Ergebnis N (0 ... 4095) */
uint16_t adcBlockMean(void);      /* Mittelwert                  */
uint16_t adcBlockLowpass(void);   /* Tiefpass, letzter Ausgangswert */

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
    uint16_t count[ADC_HIST_BINS];   /* count[i] = Anzahl Ergebnisse mit N = low + i */
} adcHist_t;

/* Eine Vorlaufumsetzung legt das Fenster fest, danach werden ADC_HIST_SAMPLES
   Umsetzungen ausgezählt. Blockiert bis zur letzten Umsetzung, gibt nichts aus.
   Danach läuft wieder der Normalbetrieb mit TB0 */
void adcHistogram(adcHist_t *h);

#endif /* SRC_INCLUDE_ADC_H_ */
