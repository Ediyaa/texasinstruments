#include <msp430.h>

#include <src/include/uart.h>

/////////////////////INIT///////////////////////////////////////
void uartInit(void){
   P4SEL |= BIT4  | BIT5;

   UCA1CTL1 |= UCSWRST;     //SoftwareReset aktivieren
   UCA1CTL1 |= UCSSEL_2;    //Taktquelle SMCLK setzen

   UCA1BRW = 9;             //Baud 115200
   UCA1MCTL = UCBRS_1;      //Timingfehler korrektur

   UCA1CTL1 &= ~UCSWRST;    //SoftwareReset deaktivieren

//   __bis_SR_register(GIE);     //Interrupts in der CPU freischalten
//
//   UCA1IE = UCRXIE;              //reiceive Byte interrupt aktiviert
};

void uartinterruptInit(void){
    __bis_SR_register(GIE);     //Interrupts in der CPU freischalten

    UCA1IE = UCRXIE;              //reiceive Byte interrupt aktiviert
}

void uartresetinterruptflag(void){
   UCA1IFG &= ~UCRXIFG;
}
