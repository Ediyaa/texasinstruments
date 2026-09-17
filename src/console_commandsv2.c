
#include "include/console.h"
#include "include/console_commandlinetools.h"
#include "include/console_commands.h"
#include "include/led.h"
#include "string.h"

/* Definiert in main.c. Gehoeren eigentlich in einen eigenen Header. */
extern void led_blink(void);
extern void led_blinksw(void);
extern void stop(void);

////////////////////////////////////////////////////////////////////////

typedef void (*commandFn)(void);

typedef struct {
    const char *name;
    commandFn   handler;   /* NULL = kein Aufruf, nur Ausgabe */
    const char *msg1;      /* NULL = keine Ausgabe           */
    const char *msg2;
} command_t;

typedef struct {
    const char      *title;
    const command_t *entries;
} commandGroup_t;

////////////////////////////////////////////////////////////////////////

static const command_t general_cmds[] = {
    { "help",  listCommands, NULL,             NULL },
    { "about", status,       NULL,             NULL },
    { "hallo", NULL,         "Hallo zurueck.", NULL },
    { "clear", clear,        NULL,             NULL },
    { NULL,    NULL,         NULL,             NULL }
};

static const command_t led_static_cmds[] = {
    { "led_rot_an",  led_rot_an,  "RED - ON",  NULL        },
    { "led_rot_aus", led_rot_aus, "RED - OFF", NULL        },
    { "led_grn_an",  led_grn_an,  "GRN - ON",  NULL        },
    { "led_grn_aus", led_grn_aus, "GRN - OFF", NULL        },
    { "led_an",      led_an,      "RED - ON",  "GRN - ON"  },
    { "led_aus",     led_aus,     "RED - OFF", "GRN - OFF" },
    { "led_switch",  led_switch,  NULL,        NULL        },
    { NULL,          NULL,        NULL,        NULL        }
};

static const command_t led_dynamic_cmds[] = {
    { "led_blink",   led_blink,   "BLINK - ON",        "press any button to cancel." },
    { "led_blinksw", led_blinksw, "blinkswitch - ON.", "press any button to cancel"  },
    { "stop",        stop,        "stop.",             NULL                          },
    { NULL,          NULL,        NULL,                NULL                          }
};

static const commandGroup_t groups[] = {
    { "General Options",     general_cmds     },
    { "LED Options Static",  led_static_cmds  },
    { "LED Options Dynamic", led_dynamic_cmds },
    { NULL,                  NULL             }
};

////////////////////////////////////////////////////////////////////////

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

            if (strcmp(eingabe, cmd[i].name) == 0){

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
        linebreak(1);
    }
}

void listCommands(void){

    int g;

    for (g = 0 ; groups[g].title != NULL ; g++){
        printGroup(&groups[g]);
    }
}
