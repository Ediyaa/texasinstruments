#include <msp430.h>
#include "include/ports.h"

void portsInit(void){
    /* Alle Pins: digitale Funktion, Ausgang, Low */
    P1SEL = 0x00;  P1OUT = 0x00;  P1DIR = 0xFF;
    P2SEL = 0x00;  P2OUT = 0x00;  P2DIR = 0xFF;
    P3SEL = 0x00;  P3OUT = 0x00;  P3DIR = 0xFF;
    P4SEL = 0x00;  P4OUT = 0x00;  P4DIR = 0xFF;
    P5SEL = 0x00;  P5OUT = 0x00;  P5DIR = 0xFF;
    P6SEL = 0x00;  P6OUT = 0x00;  P6DIR = 0xFF;
    P7SEL = 0x00;  P7OUT = 0x00;  P7DIR = 0xFF;
    P8SEL = 0x00;  P8OUT = 0x00;  P8DIR = 0xFF;
    PJOUT = 0x00;  PJDIR = 0xFF;          /* Port J hat kein SEL-Register */

    /* ADC: P6.0 als Analogeingang A0 */
    P6DIR &= ~BIT0;
    P6SEL |=  BIT0;

    /* UART UCA1: P4.4 = TXD, P4.5 = RXD */
    P4SEL |= BIT4 | BIT5;
}
