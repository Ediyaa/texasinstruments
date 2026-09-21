#include <msp430.h>
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
    uartInit();
//    uartinterruptInit();
    consoleUartInit();
    ledInit();
    timerInitA0();
    timerInitA2();
//    while(1){
//        led_switch();
//    }
    while(1){


        console_task();


    }
}
