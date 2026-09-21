#include <msp430.h>

#include "include/led.h"
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
/* urspruenglich in main.c, gehoert aber hierher: reine LED-Logik */

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
