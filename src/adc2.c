#include <msp430.h>

#include "include/adc2.h"
#include <stdint.h>

void adc2Init(void){
    __enable_interrupt();

    pinInit();        
    clkAdcInit();                        

    ADC12CTL0 &= ~ADC12ENC;                     /* Konfiguration nur bei ENC = 0 */

    ADC12CTL0  = ADC12SHT0_12                   /* 1024 Takte für ADC12MEM0     */
               | ADC12ON;                       /* ADC ein                      */

    ADC12CTL1  = ADC12CSTARTADD_0               /* Ergebnis in ADC12MEM0        */
               | ADC12SHS_0                     /* Start durch ADC12SC          */
               | ADC12SHP                       /* Abtastdauer vom Sample Timer */
               | ADC12DIV_0                     /* Teiler 1                     */
               | ADC12SSEL_0                    /* MODCLK                       */
               | ADC12CONSEQ_0;                 /* Einzelkanal, Einzelwandlung  */

    ADC12CTL2  = ADC12RES_2;                    /* 12 Bit, ADC12PDIV = 0 -> Vorteiler 1 */

    ADC12MCTL0 = ADC12SREF_0                    /* V_R+ = AV_CC, V_R- = AV_SS   */
               | ADC12INCH_11;                   /* Kanal A11             */

    ADC12CTL0 |= ADC12ENC;                      /* freigeben, zuletzt           */
}

void clkAdcInit(void){
    ADC12CTL1 &= ~(ADC12SSEL_3 | ADC12DIV_7);   /* ADC12OSC, Teiler 1 */
    ADC12CTL2 &= ~ADC12PDIV;                    /* Vorteiler 1 */
}
void pinInit(void){
    P6DIR &= ~BIT0; // P6.0 als Eingang
    P6SEL |= BIT0;  // P6.0 als ADC12_A Input
}

void readADCOn(void){
    ADC12CTL0 |= ADC12SC; // started Messung
}

uint16_t adc2Read(void){
    readADCOn();                            /* Messung starten */
    while (!(ADC12IFG & ADC12IFG0)) {       /* warten bis Ergebnis in ADC12MEM0 */
    }
    return ADC12MEM0;                       /* Lesen löscht ADC12IFG0 */
}