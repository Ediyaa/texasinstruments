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

//////////////GLOBAL_VARIABLES////////////////

volatile int led_func_stat = 0;


void led_blink(void){
    led_aus();
    led_func_stat = 1;
}
void led_blinksw(void){
    led_aus();
    led_func_stat = 2;
}
void stop(void){
    led_aus();
    led_func_stat = 0;
}



int main(void)
{
    watchdogInit();
    uartInit();
//    uartinterruptInit();
    consoleUartInit();
    ledInit();
//    timerInitA0();
//    while(1){
//        led_switch();
//    }
    while(1){


        console_task();

        while(led_func_stat >= 1){
            while(led_func_stat == 1){
                    if (checkEmpfang()){
                    led_an();
                       __delay_cycles(100000);
                       led_aus();
                       __delay_cycles(100000);
                    }
                    else{
                        stop();
                        system();
                        sends("stop.");
                        linebreak(1);
                        break;
                    }

                   }
                   while(led_func_stat == 2){
                       if (checkEmpfang()){
                       led_rot_an();
                       led_grn_aus();
                       __delay_cycles(100000);
                       led_rot_aus();
                       led_grn_an();
                       __delay_cycles(100000);
                       }
                       else{
                           stop();
                           system();
                           sends("stop.");
                           linebreak(1);
                           break;

                       }
                   }
            break;
        }
    }
}
