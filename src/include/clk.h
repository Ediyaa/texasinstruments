#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

/*
 * Taktkonfiguration MSP430F5529 LaunchPad
 * MCLK = SMCLK = DCOCLKDIV, DCO über FLL geregelt.
 * ACLK haengt in allen Varianten fest an XT1 (32768 Hz, Quarz) und
 * versorgt die UART - deshalb bleibt die Konsole beim Umschalten stehen.
 *
 *   f_DCOCLKDIV = (FLLN + 1) * f_ref / FLLREFDIV
 *   f_DCOCLK    = 2 * f_DCOCLKDIV        (FLLD = /2)
 */

/* Variante 1 und 2: Referenz 32768 Hz (XT1 bzw. REFO) -> 128 * 32768 Hz = 2^22 Hz */
#define CLOCK_FLLN_32K          127u
#define CLOCK_SETTLE_32K        (1024ul * (CLOCK_FLLN_32K + 1u))   /* 32*32*f_MCLK/f_ref */

/* Variante 3: Referenz XT2 = 4 MHz / 16 = 250 kHz -> 17 * 250 kHz = 4,25 MHz
 * (2^22 Hz ist mit 4 MHz nicht exakt erreichbar, 4,25 MHz ist der nächste Wert darüber) */
#define CLOCK_FLLN_XT2          16u
#define CLOCK_SETTLE_XT2        (1024ul * (CLOCK_FLLN_XT2 + 1u))

/* DCOCLK ≈ 8,4 MHz liegt in Bereich 4: f(4,0)max = 3,2 MHz ≤ f ≤ f(4,31)min = 12,3 MHz */
#define CLOCK_DCORSEL           DCORSEL_4

/* Interne Lastkapazität für XT1 – an den Quarz anpassen, per ACLK-Messung prüfen */
#define CLOCK_XT1_XCAP          XCAP_3

void clock_init_xt1(void);   /* DCO + FLL an XT1  (Quarz):  2^22 Hz, ppm-genau   */
void clock_init_refo(void);  /* DCO + FLL an REFO (intern): 2^22 Hz, ±3,5 %      */
void clock_init_xt2(void);   /* DCO + FLL an XT2  (Keramik): 4,25 MHz, ±0,25 %   */

#endif