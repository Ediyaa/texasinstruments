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

/* Analoge Versorgung AV_CC = V_R+ in mV (ratiometrisch, V_R- = AV_SS = 0 V).
   Nennwert des LaunchPads; geht nur in die Spannung ein, nicht in den Widerstand */
#define ADC_AVCC_MV          3300UL

/* Festwiderstand R_1 des Spannungsteilers in Ohm */
#define ADC_R1_OHM           1000UL

/* Lage von R_T im Spannungsteiler:
   1 -> AV_CC - R_1 - Mittelknoten - R_T - AV_SS  (R_T unten)
   0 -> AV_CC - R_T - Mittelknoten - R_1 - AV_SS  (R_T oben) */
#define ADC_RT_LOW_SIDE      1

/* Rückgabewert von adcToOhm, wenn R_T unendlich wäre (Division durch 0) */
#define ADC_OHM_INVALID      0xFFFFFFFFUL

/* ---------- Schnittstelle ---------- */

/* Konfiguriert P6.0, ADC12_A und Timer TB0 und startet die Abtastung */
void adcInit(void);

/* Liefert true und den neuesten Wert N (0 ... 4095), falls seit dem
   letzten Aufruf ein neuer Wert vorliegt, sonst false */
bool adcGet(uint16_t *n);

/* Liefert den zuletzt umgesetzten Wert N (0 ... 4095), ohne ihn abzuholen */
uint16_t adcLast(void);

/* Übersetzung N -> Eingangsspannung in mV */
uint16_t adcToMillivolt(uint16_t n);

/* Übersetzung N -> Widerstand R_T in Ohm (ADC_OHM_INVALID bei Division durch 0) */
uint32_t adcToOhm(uint16_t n);

#endif /* SRC_INCLUDE_ADC_H_ */
