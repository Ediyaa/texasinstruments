#include <msp430.h>

#include "include/adc2.h"
#include <stdint.h>

void adc2Init(void){
    __enable_interrupt();

    pinInit();        
    clkAdcInit();                        

    ADC12CTL0 &= ~ADC12ENC;                     /* Konfiguration nur bei ENC = 0 */

    /* Interne Referenz: Bit REFMSTR -> REF-Modul steuert die Referenz,
       Bitfeld REFVSELx -> Spannung, Bit REFON -> Referenz ein */
    REFCTL0 = REFMSTR | ADC2_REFVSEL | REFON;
    __delay_cycles(ADC2_REF_SETTLE_CYCLES);     /* Einschwingen abwarten        */

    ADC12CTL0  = ADC12SHT0_12                   /* 1024 Takte für ADC12MEM0     */
               | ADC12ON;                       /* ADC ein                      */

    ADC12CTL1  = ADC12CSTARTADD_0               /* Ergebnis in ADC12MEM0        */
               | ADC12SHS_0                     /* Start durch ADC12SC          */
               | ADC12SHP                       /* Abtastdauer vom Sample Timer */
               | ADC12DIV_1                     /* Teiler 2: f_ADC12CLK <= 2,7 MHz bei interner Referenz */
               | ADC12SSEL_3                    /* MODCLK ACLK MCLK SMCLK       */
               | ADC12CONSEQ_0;                 /* Einzelkanal, Einzelwandlung  */

    ADC12CTL2  = ADC12RES_2;                    /* 12 Bit, ADC12PDIV = 0 -> Vorteiler 1 */

    ADC12MCTL0 = ADC12SREF_1                    /* V_R+ = VREF+ (intern), V_R- = AV_SS */
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

/* ---------- Histogramm ---------- */

void adc2Histogram(adc2Hist_t *h){
    uint16_t i;
    uint16_t n;

    /* Konfiguration aus adc2Init wiederherstellen: adcmean und adclowpass (adc.c)
       stellen Bitfeld ADC12SHSx auf TB0.1 um, dann startet Bit ADC12SC keine
       Umsetzung und adc2Read() kehrt nicht zurück */
    adc2Init();

    /* Vorlaufumsetzung: legt das Fenster fest, wird nicht gezählt */
    n = adc2Read();

    h->low   = (n > ADC2_HIST_HALF) ? (uint16_t)(n - ADC2_HIST_HALF) : 0u;
    h->min   = 0xFFFFu;
    h->max   = 0u;
    h->below = 0u;
    h->above = 0u;
    for (i = 0u; i < ADC2_HIST_BINS; i++){
        h->count[i] = 0u;
    }

    for (i = 0u; i < ADC2_HIST_SAMPLES; i++){
        n = adc2Read();

        if (n < h->min) h->min = n;
        if (n > h->max) h->max = n;

        if (n < h->low){
            h->below++;
        }
        else if (n > (uint16_t)(h->low + ADC2_HIST_BINS - 1u)){
            h->above++;
        }
        else {
            h->count[n - h->low]++;
        }
    }
}
