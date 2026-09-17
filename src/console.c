#include <msp430.h>
#include <src/include/console.h>
#include <src/include/uart.h>
#include <string.h>
#include <stdbool.h>

#include "include/console_commandlinetools.h"
#include "include/console_commands.h"

//////////////Empfangspuffer///////////////////////////////////////////


#define RXBUF_SIZE 256u                  /* Zweierpotenz */
#define RXBUF_MASK (RXBUF_SIZE - 1u)

static volatile char         rxbuf[RXBUF_SIZE];
static volatile unsigned int rx_head = 0;
static volatile unsigned int rx_tail = 0;

////////////Zustand der Eingabezeile///////////////////////////////////

static char         history[HIST_MAX][LINE_MAX];
static int          hist_count = 0;     /* Anzahl gueltiger Eintraege  */
static int          browse     = 0;     /* Lesezeiger in die Historie  */

static char         cli_prompt[LINE_MAX];
static unsigned int cli_index       = 0;
static bool         cli_promptready = false;

static unsigned int esc_state = 0;      /* 0 = normal, 1 = ESC, 2 = '[' */


///////////////Ausgabe (blockierend, laeuft nur im Hauptprogramm)///////

unsigned char checkSend(void){
    return !(UCA1IFG & UCTXIFG);
}

unsigned char checkEmpfang(void){
    return !(UCA1IFG & UCRXIFG);
}

void sendc(char c){
    while (checkSend());
    UCA1TXBUF = c;
}

void sends(const char *s){
    while (*s){
        sendc(*s);
        s++;
    }
}

void consoleUartInit(void){
    uartinterruptInit();

    clear();
    sends("\033%G");
    status();
    linebreak(1);
    standardColour();
    sends("Bereit.\r\n");
    cyanfat();
    linebreak(1);
    sends(">>> ");
    standardColour();
}

/////////////////////////Zugriff auf den Empfangspuffer ////////////


unsigned char rx_available(void){
    return (rx_head != rx_tail);
}

void rx_flush(void){
    rx_tail = rx_head;
}

static char rx_get(void){
    char c = rxbuf[rx_tail];
    rx_tail = (rx_tail + 1u) & RXBUF_MASK;
    return c;
}


///////////////Zeilendisziplin/////////////////////////////////////


static void line_erase(void){
    while (cli_index > 0){
        deletechar();
        cli_index--;
    }
}

static void line_show(const char *s){
    line_erase();
    strcpy(cli_prompt, s);
    sends(cli_prompt);
    cli_index = strlen(cli_prompt);
}


///////////////////////////Historie//////////////////////////////////

static void history_add(const char *s){
    if (s[0] == '\0') return;

    if (hist_count < HIST_MAX){
        strcpy(history[hist_count], s);
        hist_count++;
    }
    else {
        int k;
        for (k = 1; k < HIST_MAX; k++){
            strcpy(history[k - 1], history[k]);
        }
        strcpy(history[HIST_MAX - 1], s);
    }
}

static void history_up(void){
    if (browse > 0){
        browse--;
        line_show(history[browse]);
    }
}

static void history_down(void){
    if (browse >= hist_count) return;

    browse++;

    if (browse == hist_count){          /* wieder leere Zeile */
        line_erase();
        cli_prompt[0] = '\0';
    }
    else {
        line_show(history[browse]);
    }
}


/*  Auswertung eines empfangenen Zeichens                              */


static void handle_char(char c){

    /* Steuersequenzen der Pfeiltasten: ESC '[' <Buchstabe> */
    if (esc_state == 1){
        esc_state = (c == 0x5B) ? 2u : 0u;
        return;
    }
    if (esc_state == 2){
        esc_state = 0;
        if      (c == 'A') history_up();
        else if (c == 'B') history_down();
        return;                         /* 'C' und 'D' werden verworfen */
    }
    if (c == 0x1B){
        esc_state = 1;
        return;
    }

    if (c == '\r' || c == '\n'){
        cli_prompt[cli_index] = '\0';
        history_add(cli_prompt);
        browse = hist_count;
        cli_index = 0;
        cli_promptready = true;
    }
    else if (c == '\b' || c == 0x7F){   //0x7F = DEL
        if (cli_index > 0){
            cli_index--;
            deletechar();
        }
    }
    else if (c >= 0x20 && c <= 0x7E){
        if (cli_index < LINE_MAX - 1){
            cli_prompt[cli_index++] = c;
            sendc(c);
        }
    }
}

//  Wird zyklisch aus main() gerufen

void console_task(void){

    while (rx_available()){
        handle_char(rx_get());
    }

    if (cli_promptready){
        cli_promptready = false;
        linebreak(1);
        commands(cli_prompt);
        cyanfat();
        linebreak(1);
        sends(">>> ");
        standardColour();
    }
}

/*  Unterbrechungsroutine: legt das Byte nur ab                        */

#pragma vector = USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void){
    char c = UCA1RXBUF;
    unsigned int next = (rx_head + 1u) & RXBUF_MASK;

    if (next != rx_tail){               /* voll: Zeichen verwerfen */
        rxbuf[rx_head] = c;
        rx_head = next;
    }
}
