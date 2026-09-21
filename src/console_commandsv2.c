#include <msp430.h>

#include "include/console.h"
#include "include/console_commandlinetools.h"
#include "include/console_commands.h"
#include "include/led.h"
#include "include/timer.h"
#include "string.h"
#include <stdbool.h>

////////////////////////////////////////////////////////////////////////

typedef void (*commandFn)(void);
typedef void (*commandArgFn)(unsigned int);

typedef struct {
    const char  *name;
    commandFn    handler;     /* NULL = kein Aufruf, nur Ausgabe        */
    commandArgFn argHandler;  /* != NULL = Befehl erwartet eine Zahl    */
    const char  *argHint;     /* Hilfetext bei argHandler, z.B. "<zahl>"*/
    const char  *msg1;        /* NULL = keine Ausgabe                   */
    const char  *msg2;
} command_t;

typedef struct {
    const char      *title;
    const command_t *entries;
} commandGroup_t;

////////////////////////////////////////////////////////////////////////

static const command_t general_cmds[] = {
    { "help",  listCommands, NULL, NULL, NULL,             NULL },
    { "about", status,       NULL, NULL, NULL,             NULL },
    { "hallo", NULL,         NULL, NULL, "Hallo zurueck.", NULL },
    { "clear", clear,        NULL, NULL, NULL,             NULL },
    { NULL,    NULL,         NULL, NULL, NULL,             NULL }
};

static const command_t led_static_cmds[] = {
    { "led_rot_an",  led_rot_an,  NULL, NULL, "RED - ON",  NULL        },
    { "led_rot_aus", led_rot_aus, NULL, NULL, "RED - OFF", NULL        },
    { "led_grn_an",  led_grn_an,  NULL, NULL, "GRN - ON",  NULL        },
    { "led_grn_aus", led_grn_aus, NULL, NULL, "GRN - OFF", NULL        },
    { "led_an",      led_an,      NULL, NULL, "RED - ON",  "GRN - ON"  },
    { "led_aus",     led_aus,     NULL, NULL, "RED - OFF", "GRN - OFF" },
    { "led_switch",  led_switch,  NULL, NULL, NULL,        NULL        },
    { NULL,          NULL,        NULL, NULL, NULL,        NULL        }
};

static const command_t led_dynamic_cmds[] = {
    { "led_blink",   led_blink,   NULL, NULL, "BLINK - ON",        "press any button to cancel." },
    { "led_blinksw", led_blinksw, NULL, NULL, "blinkswitch - ON.", "press any button to cancel"  },
    { "stop",        stop,        NULL, NULL, "stop.",             NULL                          },
    { NULL,          NULL,        NULL, NULL, NULL,                NULL                          }
};

static void setTimer(unsigned int wert);
static void setDimmf(unsigned int wert);
static void setDimm(unsigned int wert);
static void statuspwm(void);


static const command_t timer_cmds[] = {
    { "settimer_", NULL, setTimer, "<INTEGER[1,100]>",        NULL, NULL },
    { "setdimmf_",  NULL, setDimmf,  "<INTEGER[1,4]>", NULL, NULL },
    { "setdimm_",   NULL, setDimm,   "<INTEGER[0,100]>", NULL, NULL },
    { "statuspwm",  statuspwm, NULL,    NULL, NULL, NULL },
    { "pwmtoggle",  pwmtoggle,      NULL, NULL, NULL,           NULL },
    { NULL,        NULL, NULL,     NULL,            NULL, NULL }
};

static const commandGroup_t groups[] = {
    { "General Options",     general_cmds     },
    { "LED Options Static",  led_static_cmds  },
    { "LED Options Dynamic", led_dynamic_cmds },
    { "Timer Options",       timer_cmds       },
    { NULL,                  NULL             }
};

////////////////////////////////////////////////////////////////////////

/* Zahl ueber sends() ausgeben. int ist auf dem MSP430 16 Bit,
 * unsigned also maximal 65535 -> 5 Ziffern plus Terminator.
 * Nicht mehr static: console_commandlinetools.c (pwmstatus) braucht sie auch. */
void sendNum(unsigned int n){

    char buf[6];
    int  i = 5;

    buf[i] = '\0';

    do {
        buf[--i] = (char)('0' + (n % 10u));
        n /= 10u;
    } while (n != 0u);

    sends(&buf[i]);
}

/* Ziffernfolge einlesen. Liefert false bei leerer Eingabe,
 * Nicht-Ziffern oder Ueberlauf jenseits von 65535. */
static bool parseUInt(const char *s, unsigned int *out){

    unsigned int wert = 0u;

    if (*s == '\0') return false;

    for ( ; *s != '\0' ; s++){

        if (*s < '0' || *s > '9') return false;

        if (wert > 6553u) return false;
        wert *= 10u;

        if (wert > (65535u - (unsigned int)(*s - '0'))) return false;
        wert += (unsigned int)(*s - '0');
    }

    *out = wert;
    return true;
}

static void respond(const char *msg){
    system();
    sends(msg);
    linebreak(1);
}

void commands(char *eingabe){

    int g, i;

    for (g = 0 ; groups[g].title != NULL ; g++){

        const command_t *cmd = groups[g].entries;

        for (i = 0 ; cmd[i].name != NULL ; i++){

            if (cmd[i].argHandler != NULL){

                /* Befehl mit Argument: Praefix vergleichen, Rest als Zahl lesen. */
                size_t len = strlen(cmd[i].name);

                if (strncmp(eingabe, cmd[i].name, len) == 0){

                    unsigned int wert;

                    if (parseUInt(&eingabe[len], &wert)){
                        cmd[i].argHandler(wert);
                    }
                    else {
                        system();
                        sends("invalid argument for: ");
                        red();
                        sends(cmd[i].name);
                        linebreak(1);
                    }
                    return;
                }
            }
            else if (strcmp(eingabe, cmd[i].name) == 0){

                if (cmd[i].handler != NULL) cmd[i].handler();
                if (cmd[i].msg1    != NULL) respond(cmd[i].msg1);
                if (cmd[i].msg2    != NULL) respond(cmd[i].msg2);

                return;
            }
        }
    }

    system();
    sends("command not found: ");
    red();
    sends("\"");
    sends(eingabe);
    sends("\"");
    linebreak(1);
}

////////////////////////////////////////////////////////////////////////

static void printGroup(const commandGroup_t *gruppe){

    int i;

    whitefat();
    sends(gruppe->title);
    linebreak(1);
    treeBeginn();
    linebreak(1);

    for (i = 0 ; gruppe->entries[i].name != NULL ; i++){

        if (gruppe->entries[i + 1].name != NULL){
            treeMiddle();
        }
        else {
            treeEnd();
        }

        cyan();
        sends(gruppe->entries[i].name);

        if (gruppe->entries[i].argHint != NULL){
            sends(gruppe->entries[i].argHint);
        }

        linebreak(1);
    }
}

void listCommands(void){

    int g;

    for (g = 0 ; groups[g].title != NULL ; g++){
        printGroup(&groups[g]);
    }
}

////////////////////////////////////////////////////////////////////////

static void setTimer(unsigned int wert){


    if (wert < 1 || wert > 100){
        system();
        sends("invalid argument for: ");
        red();
        sends("settimer_");
        linebreak(1);
        return;
    }

    system();
    sends("timer set to: ");
    cyan();
    sendNum(wert);
    standardColour();
    linebreak(1);
}

static void setDimmf(unsigned int wert){

    static const char *const freq[] = { "128Hz","256Hz","512Hz", "1024Hz", "2048Hz", "4096Hz", "8192Hz" };

    if (wert < 1 || wert > 7){
        system();
        sends("invalid argument for: ");
        red();
        sends("setdimmf_");
        linebreak(1);
        return;
    }

    timerSetDimmf(wert);

    system();
    sends("dimm frequency set to: ");
    cyan();
    sends(freq[wert - 1]);
    standardColour();
    linebreak(1);
}

static void setDimm(unsigned int wert){

    if (wert < 0 || wert > 100){
        system();
        sends("invalid argument for: ");
        red();
        sends("setdimm_");
        linebreak(1);
        return;
    }

    timersetDimm(wert);

    system();
    sends("dimm set to: ");
    cyan();
    sendNum(wert);
    standardColour();
    linebreak(1);
}
static void statuspwm(void){
    pwmstatus();
}
