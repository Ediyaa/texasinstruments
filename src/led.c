#include <msp430.h>

#include "include/led.h"
#include "include/timer.h"
/*
 * led.c
 *
 *  Created on: 02.09.2026
 *      Author: edwinaab
 */


/////////////////////INIT///////////////////////////////////////
void ledInit(void){



    P1DIR |= BIT0;      //LED Rot   - Port 1.0 als Ausgang
    P4DIR |= BIT7;      //LED Gr�n  - Port 4.7 als Ausgang
}

void ledInitExtern(void){

    P2DIR |= BIT5;      //LED Rot   - Port 1.0 als Ausgang
    

 
}
////////////////////////////////////////////////////////////////
void led_rot_an(void){
    P1OUT |= BIT0;
}

void led_grn_an(void){
    P4OUT |= BIT7;
}

void led_rot_aus(void){
    P1OUT &= ~BIT0;
}

void led_grn_aus(void){
    P4OUT &= ~BIT7;
}
void led_an(void){
    led_rot_an();
    led_grn_an();
}
void led_aus(void){
    led_rot_aus();
    led_grn_aus();
}
void led_switch(void){
    if (!(P4OUT == BIT7)&&(P1OUT == BIT0)){
        led_rot_aus();
        led_grn_an();
    }
    else if ((P4OUT == BIT7)&&!(P1OUT == BIT0)){
        led_rot_an();
        led_grn_aus();
    }
    else if ((P4OUT == BIT7)&&(P1OUT == BIT0)){
        led_rot_aus();
        led_grn_aus();
    }
    else if (!(P4OUT == BIT7)&&!(P1OUT == BIT0)){
        led_rot_an();
        led_grn_an();
    }
}

//////////////////DYNAMISCHER BLINK-MODUS/////////////////////////


volatile int led_func_stat = 0;

void led_blink(void){
    led_aus();
    led_func_stat = 1;
}
void led_blinksw(void){
    led_aus();
    led_func_stat = 2;
}
void stop(void){
    led_aus();
    led_func_stat = 0;
}


unsigned int perceived_to_duty(unsigned int wert, unsigned int ccr0)
{
    unsigned long v, y;

    if (wert <= 800u) {                 /* L* <= 8: linearer Fuss */
        return (unsigned int)(((unsigned long)wert * ccr0) / 90330uL);
    }

    v = ((unsigned long)(wert + 1600u) * 4096uL) / 11600uL;   /* (L*+16)/116 in Q12 */
    y = (v * v) >> 12;
    y = (y * v) >> 12;                  /* hoch drei */

    return (unsigned int)((y * ccr0) >> 12);
}

void ledfade(void){
    static unsigned int wert = 1;
    static int richtung = 1;
while (1){
    __delay_cycles(100);
    if (richtung == 1){
        wert++;
        if (wert >= 10000){
            richtung = -1;
        }
    }
    else{
        wert--;
        if (wert == 0){
            richtung = 1;
        }
    }
    if (wert == 0){
        return;
    }
    timersetDimm(perceived_to_duty(wert, 10000u));
}

    
}