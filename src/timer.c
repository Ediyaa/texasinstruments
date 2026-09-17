#include <msp430.h>
#include "include/timer.h"
#include "include/led.h"
#include <src/include/console.h>

///////////////////REGISTER//////////////////////////
int toggle = 0;

void timerInitA0(void){

    TA0CCR0 = 1024;
    TA0CCR1 = 512;

    TA0CTL = TASSEL__SMCLK | ID_0 | MC_1 | TACLR;

    TA0CCTL0 |= CCIE;
    TA0CCTL1 |= CCIE;

//    TA0CTL |= TAIE;

    __enable_interrupt();
}

void timerSetDimmf(unsigned int wert){

    if (wert == 1){
        TA0CCR0 = 1024;
        TA0CCR1 = 512;
    }
    else if (wert == 2){
        TA0CCR0 = 512;
        TA0CCR1 = 256;
    }
    else if (wert == 3){
        TA0CCR0 = 256;
        TA0CCR1 = 128;
    }
    else if (wert == 4){
        TA0CCR0 = 128;
        TA0CCR1 = 64;
    }
    else return;
}

void timersetDimm (unsigned int wert){
    int pwmf = TA0CCR0;

    if (wert < 1 || wert > 100){
        return;
    }

    TA0CCR1 = (TA0CCR0 * wert + 50) / 100;
    return;

}
void ledblinktoggle(void){
    if (toggle == 0){
        toggle = 1;
    }
    else{
        toggle = 0;
    }

}

//A0_0
#pragma vector = TIMER0_A0_VECTOR
__interrupt void ta0_ccr0_isr(void){

    if (toggle == 1){
        led_an();
    }

}
//A0_1
#pragma vector = TIMER0_A1_VECTOR
__interrupt void ta0_ccr1_isr(void){

    if (toggle == 1){
        led_aus();
    } 
    TA0CCTL1 &= ~CCIFG;

}
