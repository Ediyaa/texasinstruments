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

/* ---------- Schnittstelle ---------- */

/* Konfiguriert P6.0, ADC12_A und Timer TB0 und startet die Abtastung */
void adcInit(void);

/* Liefert true und den neuesten Wert N (0 ... 4095), falls seit dem
   letzten Aufruf ein neuer Wert vorliegt, sonst false */
bool adcGet(uint16_t *n);

/* Liefert den zuletzt umgesetzten Wert N (0 ... 4095), ohne ihn abzuholen */
uint16_t adcLast(void);

#endif /* SRC_INCLUDE_ADC_H_ */
