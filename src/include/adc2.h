#ifndef SRC_INCLUDE_ADC2_H_
#define SRC_INCLUDE_ADC2_H_

#include <stdint.h>

/* ---------- Histogramm ---------- */

/* Anzahl der Umsetzungen, die ausgezählt werden */
#define ADC2_HIST_SAMPLES   1024u

/* Halbe Fensterbreite in Stufen: das Fenster reicht vom Ergebnis der
   Vorlaufumsetzung minus ADC2_HIST_HALF bis plus ADC2_HIST_HALF */
#define ADC2_HIST_HALF      16u

/* Anzahl der Klassen im Fenster */
#define ADC2_HIST_BINS      (2u * ADC2_HIST_HALF + 1u)

typedef struct {
    uint16_t low;                     /* Ergebnis N, das zu count[0] gehört          */
    uint16_t min;                     /* kleinstes Ergebnis                           */
    uint16_t max;                     /* größtes Ergebnis                             */
    uint16_t below;                   /* Anzahl Ergebnisse unterhalb des Fensters     */
    uint16_t above;                   /* Anzahl Ergebnisse oberhalb des Fensters      */
    uint16_t count[ADC2_HIST_BINS];   /* count[i] = Anzahl Ergebnisse mit N = low + i */
} adc2Hist_t;


void adc2Init(void);
void clkAdcInit(void);
void pinInit(void);
void readADCOn(void);
uint16_t adc2Read(void);

/* Eine Vorlaufumsetzung legt das Fenster fest, danach werden
   ADC2_HIST_SAMPLES Umsetzungen mit adc2Read() ausgezählt.
   Blockiert bis zur letzten Umsetzung, gibt nichts aus */
void adc2Histogram(adc2Hist_t *h);

#endif /* SRC_INCLUDE_ADC2_H_ */
