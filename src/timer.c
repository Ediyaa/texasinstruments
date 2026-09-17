#include <msp430.h>
#include "include/timer.h"
#include "include/led.h"
#include <src/include/console.h>

///////////////////REGISTER//////////////////////////

void timerInitA0(void){

    TA0CCR0 = 1024;
    TA0CCR1 = 512;

    TA0CTL = TASSEL__SMCLK | ID_0 | MC_1 | TACLR;

    TA0CCTL0 |= CCIE;
    TA0CCTL1 |= CCIE;

//    TA0CTL |= TAIE;

    __enable_interrupt();
}
//A0_0
#pragma vector = TIMER0_A0_VECTOR
__interrupt void ta0_ccr0_isr(void){


    led_an();
//    sends("1");
}
//A0_1
#pragma vector = TIMER0_A1_VECTOR
__interrupt void ta0_ccr1_isr(void){

    led_aus();
    TA0CCTL1 &= ~CCIFG;
//    sends("1");
}
