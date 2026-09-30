#include <msp430.h>

#include "include/adc.h"
#include "include/clk.h"
#include "include/ports.h"
#include "include/timer.h"
#include <src/include/console.h>
#include <src/include/uart.h>
#include <string.h>
#include <stdbool.h>

#include "include/console_commandlinetools.h"
#include "include/console_commands.h"


#include "include/led.h"


void watchdogInit(void){
    WDTCTL = WDTPW | WDTHOLD;
}

int main(void)
{
    watchdogInit();
    portsInit();
    clock_init_xt1();               /* MCLK = SMCLK = 2^22 Hz, ACLK = XT1 (clk.h) */
    uartInit();    
    consoleUartInit();

    
    uartinterruptInit();

    ledInit();
    timerInitA0();
//     // timerInitA2();
    adcInit();
//   while(1){
//        led_switch();
//    }
    uint16_t adcValue;

    while(1){


        console_task();

        if (adcGet(&adcValue)) {
            /* Verarbeitung von adcValue (0 ... 4095) folgt später */
            if (adcstreamflag) {
                adcPrint(adcValue);
                linebreak(1);
            }
        }


    }
}
