/*
 * led.h
 *
 *  Created on: 02.09.2026
 *      Author: edwinaab
 */

#ifndef INLCUDE_LED_H_
#define INLCUDE_LED_H_

void ledInit(void);

void led_rot_an(void);

void led_grn_an(void);

void led_rot_aus(void);

void led_grn_aus(void);

void led_an(void);

void led_aus(void);

void led_switch(void);

/* Dynamischer Blink-Modus, urspruenglich in main.c */
extern volatile int led_func_stat;

void led_blink(void);
void led_blinksw(void);
void stop(void);

#endif /* INLCUDE_LED_H_ */
