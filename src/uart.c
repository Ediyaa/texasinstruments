#include <msp430.h>

#include <src/include/uart.h>

/////////////////////INIT///////////////////////////////////////
void uartInit(void){
   P4SEL |= BIT4  | BIT5;

   UCA1CTL1 |= UCSWRST;     //SoftwareReset aktivieren
   UCA1CTL1 |= UCSSEL_1;    //Taktquelle ACLK setzen (XT1, 32768 Hz, quarzgenau)

   UCA1BRW = 3;             //Baud 9600: 32768 / 9600 = 3,41
   UCA1MCTL = UCBRS_3;      //Timingfehler korrektur

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
