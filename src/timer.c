#include <msp430.h>
#include "include/timer.h"
#include "include/led.h"
#include <src/include/console.h>
#include "include/console_commandlinetools.h"

///////////////////REGISTER//////////////////////////
volatile int pwmtoggleflag = 0;

void timerInitA0(void){

    TA0CCR0 = 1024;
    TA0CCR1 = 512;

    TA0CTL = TASSEL__SMCLK | ID_0 | MC_1 | TACLR;

    TA0CCTL0 |= CCIE;
    TA0CCTL1 |= CCIE;

//    TA0CTL |= TAIE;

    __enable_interrupt();
}

void timerInitA2(void){

    P2DIR |= BIT5;                    
    P2SEL |= BIT5;                   

    P2DS |= BIT5;

    TA2CCR0  = 1023;                  
    TA2CCR2  = 512;                   
    TA2CCTL2 = OUTMOD_7;              
                                    

    TA2CTL = TASSEL__SMCLK | ID__1 | MC__UP | TACLR;   // SMCLK, kein Teiler, Up-Modus
}

void timerSetDimmf(unsigned int wert){

    if (wert == 1){
        TA0CCR0 = 128;
        TA0CCR1 = 64;

        TA2CCR0  = 128;                  
        TA2CCR2  = 64; 
    }

    else if (wert == 2){
        TA0CCR0 = 256;
        TA0CCR1 = 128;

        TA2CCR0  = 256;                  
        TA2CCR2  = 128; 
    }
    else if (wert == 3){
        TA0CCR0 = 512;
        TA0CCR1 = 256;

        TA2CCR0  = 512;                  
        TA2CCR2  = 256; 
    }
    else if (wert == 4){
        TA0CCR0 = 1024;
        TA0CCR1 = 512;

        TA2CCR0  = 1023;                  
        TA2CCR2  = 512; 
    }
    else if (wert == 5){
        TA0CCR0 = 2048;
        TA0CCR1 = 1024;

        TA2CCR0  = 2048;                  
        TA2CCR2  = 1024; 
    }
    else if (wert == 6){
        TA0CCR0 = 4096;
        TA0CCR1 = 2048;

        TA2CCR0  = 4096;                  
        TA2CCR2  = 2048; 
    }
       else if (wert == 7){
        TA0CCR0 = 8192;
        TA0CCR1 = 4096;

        TA2CCR0  = 8192;                  
        TA2CCR2  = 4096; 
    }
   
    else return;
}

void timersetDimm (unsigned int wert){

    if (wert < 0 || wert > 100){
        return;
    }

    /* 32 Bit rechnen: TA0CCR0 * wert sprengt sonst die 16 Bit
     * (z.B. 1024 * 100 = 102400 > 65535). +50 rundet kaufmaennisch. */
    if (wert == 0){
        TA0CCR1 = 1;
        TA2CCR2  = 1;
        return;
    }
    else
    TA0CCR1 = (unsigned int)(((unsigned long)TA0CCR0 * wert + 50uL) / 100uL);
    TA2CCR2  = (unsigned int)(((unsigned long)TA2CCR0 * wert + 50uL) / 100uL);
}
void pwmtoggle(void){
    if (pwmtoggleflag == 0){
        pwmtoggleflag = 1;
        system();
        sends("PWM - ON");
    }
    else{
        pwmtoggleflag = 0;
        system();
        sends("PWM - OFF");
    }
    linebreak(1);
}

void pwmstatus(void){
    blue();
    sends("PWM: ");
    if (pwmtoggleflag == 1){
        sends("PWM is enabled\r\n");
    }
    else {
        sends("PWM is disabled\r\n");
    }
    sends("Frequency:   ");
    sendNum(TA0CCR0);
    standardColour();
    sends("\r\n");
    sends("Duty:    ");
    sendNum(TA0CCR1);
    standardColour();
    sends("\r\n");
}

//A0_0
#pragma vector = TIMER0_A0_VECTOR
__interrupt void ta0_ccr0_isr(void){

    if (pwmtoggleflag == 1){
        led_an();
    }

}
//A0_1
#pragma vector = TIMER0_A1_VECTOR
__interrupt void ta0_ccr1_isr(void){

    if (pwmtoggleflag == 1){
        led_aus();
    } 
    TA0CCTL1 &= ~CCIFG;
    

}
