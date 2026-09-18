#include <msp430.h>
#include "include/console.h"
#include "include/console_commandlinetools.h"
#include "include/timer.h"


void clear(void){
    sends("\033[2J");
    sends("\033[3J");
    sends("\033[H");
}

void linebreak(int n){
    int i;
    for(i = 0; i<n ; i++){
        sends("\r\n");
    }
}

void deletechar(void){
    sendc('\b'); sendc(' '); sendc('\b');
}

void space(void){
    sends("   ");
}

void system(void){
    sends("System:");
    space();
}

void treeBeginn(void){
//    sends("\xe2\x94\220");
//    linebreak(1);
    whitefat();
    sends("\xe2\x94\x82");
}

void treeMiddle(void){
    whitefat();
    sends("\xe2\x94\x9c");
}

void treeEnd(void){
    whitefat();
    sends("\xe2\x94\x94");
}

void status(void){
    blue();
    sends("Manufac: TexasInstruments\r\n");
    sends("Model:   F5529\r\n");
    sends("Clock:   1048576\r\n");
    sends("Baud:    115200\r\n");
    sends("Format:  UTF-8\r\n");
}


void standardColour(void){
    sends("\033[0m");
}
void grey(void){
    sends("\033[90m");
}
void green(void){
    sends("\033[32m");
}
void yellow(void){
    sends("\033[33m");
}
void red(void){
    sends("\033[38;5;203m");
}
void blue(void){
    sends("\033[38;5;111m");
}
void whitefat(void){
    sends("\033[1;37m");
}
void cyan(void){
    sends("\033[36m");
}
void cyanfat(void){
    sends("\033[1;36m");
}
