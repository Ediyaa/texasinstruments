#include <msp430.h>

#include "include/console.h"
#include "include/console_commandlinetools.h"
#include "include/console_commands.h"
#include "include/led.h"
#include "include/timer.h"
#include "include/clk.h"
#include "include/adc.h"
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
static void setTimer(unsigned int wert);
static void setDimmf(unsigned int wert);
static void setDimm(unsigned int wert);
static void setDimmlin(unsigned int wert);
static void statuspwm(void);

static void setClock(unsigned int wert);
static void statusclock(void);
static void led_fade(void);

static void adcRead(void);
static void adcStream(void);
static void adcUnit(unsigned int wert);
static void adcMean(void);
static void adcLowpass(void);
static void adcHist(void);

volatile bool adcstreamflag = false;

/* Ausgabeeinheit für adcread und adcstream: 0 = N, 1 = mV, 2 = Ohm, 3 = °C */
static unsigned int adcunit = 0u;


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
    { "led_fade",    led_fade,    NULL, NULL, "fadecycle",             NULL                          },
    { NULL,          NULL,        NULL, NULL, NULL,                NULL                          }
};


static const command_t timer_cmds[] = {
    { "settimer_", NULL, setTimer, "<INTEGER[1,100]>",        NULL, NULL },
    { "setdimmf_",  NULL, setDimmf,  "<INTEGER[0,7]>", NULL, NULL },
    { "setdimm_",   NULL, setDimm,   "<INTEGER[0,65536]>", NULL, NULL },
    { "setdimmlin_",   NULL, setDimmlin,   "<INTEGER[0,100]>", NULL, NULL },
    { "statuspwm",  statuspwm, NULL,    NULL, NULL, NULL },
    { "pwmtoggle",  pwmtoggle,      NULL, NULL, NULL,           NULL },
    { NULL,        NULL, NULL,     NULL,            NULL, NULL }
};

static const command_t clock_cmds[] = {
    { "setclock_", NULL, setClock, "<INTEGER[1,3]>",        NULL, NULL },
    { "statusclock",  statusclock, NULL,    NULL, NULL, NULL },
    { NULL,        NULL, NULL,     NULL,            NULL, NULL }
};

static const command_t adc_cmds[] = {
    { "adcread",   adcRead,   NULL, NULL, NULL, NULL },
    { "adcstream", adcStream, NULL, NULL, NULL, NULL },
    { "adcunit_",  NULL,      adcUnit, "<INTEGER[0,3]> 0=N 1=mV 2=Ohm 3=degC", NULL, NULL },
    { "adcmean",   adcMean,   NULL, NULL, NULL, NULL },
    { "adclowpass", adcLowpass, NULL, NULL, NULL, NULL },
    { "adchist",   adcHist,   NULL, NULL, NULL, NULL },
    { NULL,        NULL,      NULL, NULL, NULL, NULL }
};

static const commandGroup_t groups[] = {
    { "General Options",     general_cmds     },
    { "LED Options Static",  led_static_cmds  },
    { "LED Options Dynamic", led_dynamic_cmds },
    { "Timer Options",       timer_cmds       },
    { "Clock Options",       clock_cmds       },
    { "ADC Options",         adc_cmds         },
    { NULL,                  NULL             }
};

////////////////////////////////////////////////////////////////////////

/* Zahl ueber sends() ausgeben. int ist auf dem MSP430 16 Bit,
 * unsigned also maximal 65535 -> 5 Ziffern plus Terminator.
 * Nicht mehr static: console_commandlinetools.c (pwmstatus) braucht sie auch. */

static void statusclock(void){
    unsigned int selref = UCSCTL3 & 0x0070;      /* SELREF, Bit 6-4 */

    system();
    if (UCSCTL7 & (XT1LFOFFG | DCOFFG)) {        /* Quelle ausgefallen -> Fail-safe */
        sends("Clock:   fail-safe (REFO), ungeregelt");
    }
    else if (selref == SELREF__XT1CLK) {
        sends("Clock:   XT1 (quartz), 4 194 304 Hz, +/-0,002%");
    }
    else if (selref == SELREF__REFOCLK) {
        sends("Clock:   REFO (internal), 4 194 304 Hz, +/-3,5%");
    }
    else if (selref == SELREF__XT2CLK) {
        sends("Clock:   XT2 (ceramic), 4 250 000 Hz, +/-0,25%");
    }
    else {
        sends("Clock:   unknown");
    }
    linebreak(1);
}

static void setClock(unsigned int wert){
    if (wert == 1){
        clock_init_refo();
        system();
        sends("Clock set to: REFO (internal), 4 194 304 Hz, ±3,5%");
        linebreak(1);
    }
    else if (wert == 2){
        clock_init_xt1();
        system();
        sends("Clock set to: XT1 (quartz), 4 194 304 Hz, ±0,002%");
        linebreak(1);
    }
    else if (wert == 3){
        clock_init_xt2();
        system();
        sends("Clock set to: XT2 (ceramic), 4 250 000 Hz, ±0,25%, ");
        linebreak(1);
    }
    else{
        system();
        sends("invalid argument for: ");
        red();
        sends("setclock_");
        linebreak(1);
    }
}

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

/* wie sendNum, aber 32 Bit: maximal 4294967295 -> 10 Ziffern plus Terminator */
void sendNumL(unsigned long n){

    char buf[11];
    int  i = 10;

    buf[i] = '\0';

    do {
        buf[--i] = (char)('0' + (n % 10uL));
        n /= 10uL;
    } while (n != 0uL);

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

    static const char *const freq[] = { "128Hz","256Hz","512Hz","1024Hz", "2048Hz", "4096Hz", "8192Hz" };

    if (wert < 0 || wert > 7){
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
    sends(freq[wert]);
    standardColour();
    linebreak(1);
}

static void setDimm(unsigned int wert){

    if (wert < 0 || wert > 65535){
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

static void setDimmlin(unsigned int wert){

    if (wert > 100u){
        system();
        sends("invalid argument for: ");
        red();
        sends("setdimmlin_");
        sendNum(wert);
        linebreak(1);
        return;
    }

    /* wert ist L* in Prozent (0..100) -> *100 ergibt die 0..10000, die
     * perceived_to_duty erwartet. ccr0 = 10000, damit das Ergebnis
     * bereits in der Einheit vorliegt, die timersetDimm entgegennimmt. */
    unsigned int dimmwert = perceived_to_duty(wert * 100u, 10000u);

    timersetDimm(dimmwert);

    system();
    sends("dimm set to: ");
    cyan();
    sendNum(dimmwert);
    standardColour();
    linebreak(1);
}

static void statuspwm(void){
    pwmstatus();
}

static void led_fade(void){
    ledfade();
}

/* Wert in der mit adcunit_ gewählten Einheit ausgeben (ohne Farbe/Zeilenumbruch) */
void adcPrint(uint16_t n){

    if (adcunit == 1u){
        sendNum(adcToMillivolt(n));
        sends(" mV");
    }
    else if (adcunit == 2u){
        uint32_t r = adcToOhm(n);
        if (r == ADC_OHM_INVALID){
            sends("inf Ohm");
        }
        else {
            sendNumL(r);
            sends(" Ohm");
        }
    }
    else if (adcunit == 3u){
        int32_t t = adcToCentiCelsius(n);
        if (t == ADC_TEMP_INVALID){
            sends("invalid");
        }
        else {
            if (t < 0L){
                sendc('-');
                t = -t;
            }
            sendNumL((unsigned long)(t / 100L));
            sendc('.');
            if ((t % 100L) < 10L){
                sendc('0');
            }
            sendNum((unsigned int)(t % 100L));
            sends(" \xc2\xb0""C");       /* UTF-8 Gradzeichen */
        }
    }
    else {
        sendNum(n);
    }
}

/* Letzten Wert einmal ausgeben */
static void adcRead(void){
    system();
    sends("ADC: ");
    cyan();
    adcPrint(adcLast());
    standardColour();
    linebreak(1);
}

/* Ausgabeeinheit wählen */
static void adcUnit(unsigned int wert){

    static const char *const unit[] = { "N (raw)", "mV", "Ohm", "\xc2\xb0""C" };

    if (wert > 3u){
        system();
        sends("invalid argument for: ");
        red();
        sends("adcunit_");
        linebreak(1);
        return;
    }

    adcunit = wert;

    system();
    sends("ADC unit set to: ");
    cyan();
    sends(unit[wert]);
    standardColour();
    linebreak(1);
}

/* Fortlaufende Ausgabe jedes neuen Werts ein- bzw. ausschalten */
static void adcStream(void){
    adcstreamflag = !adcstreamflag;
    system();
    sends(adcstreamflag ? "ADC stream - ON" : "ADC stream - OFF");
    linebreak(1);
}

/* Gleitenden Mittelwert für adcstream, adcread und adchist ein- bzw. ausschalten */
static void adcMean(void){
    bool on = adcMeanToggle();
    system();
    sends(on ? "ADC mean filter - ON" : "ADC mean filter - OFF");
    linebreak(1);
}

/* Tiefpass für adcstream, adcread und adchist ein- bzw. ausschalten */
static void adcLowpass(void){
    bool on = adcLowpassToggle();
    system();
    sends(on ? "ADC lowpass filter - ON" : "ADC lowpass filter - OFF");
    linebreak(1);
}

/* Zahl rechtsbündig in einem Feld der Breite w ausgeben */
static void sendNumPad(unsigned int n, unsigned int w){
    unsigned int stellen = 1u;
    unsigned int t = n;

    while (t >= 10u){
        t /= 10u;
        stellen++;
    }
    while (w > stellen){
        sendc(' ');
        w--;
    }
    sendNum(n);
}

/* Histogramm aufnehmen und als Balkendiagramm ausgeben.
   Die Ausgabe beginnt erst nach der letzten Umsetzung, damit die
   serielle Ausgabe nicht in die Messung fällt */
static void adcHist(void){
    static adcHist_t h;                   /* nicht auf dem Stack */
    uint16_t i;
    uint16_t j;
    uint16_t first = ADC_HIST_BINS;      /* erste belegte Klasse    */
    uint16_t last  = 0u;                  /* letzte belegte Klasse   */
    uint16_t peak  = 0u;                  /* größte Anzahl je Klasse */
    uint16_t bar;

    /* Bit UCBUSY: warten, bis die letzten Zeichen gesendet sind */
    while (UCA1STAT & UCBUSY){
    }

    adcHistogram(&h);

    for (i = 0u; i < ADC_HIST_BINS; i++){
        if (h.count[i] != 0u){
            if (first == ADC_HIST_BINS) first = i;
            last = i;
            if (h.count[i] > peak) peak = h.count[i];
        }
    }

    system();
    sends("ADC histogram, samples: ");
    cyan();
    sendNum(ADC_HIST_SAMPLES);
    standardColour();
    sends(", filter: ");
    cyan();
    if (!adcMeanGet() && !adcLowpassGet()){
        sends("none");
    }
    if (adcMeanGet()){
        sends("mean ");
    }
    if (adcLowpassGet()){
        sends("lowpass");
    }
    standardColour();
    linebreak(1);

    /* Nur von der ersten bis zur letzten belegten Klasse. Liegen alle
       Ergebnisse außerhalb des Fensters, ist first > last und die
       Schleife läuft nicht (peak = 0 wird dann nicht als Teiler benutzt) */
    for (i = first; i <= last; i++){
        sendNumPad((unsigned int)(h.low + i), 4u);
        sends(" |");
        sendNumPad(h.count[i], 5u);
        sendc(' ');

        /* aufgerundet, damit jede belegte Klasse mindestens ein Zeichen hat */
        bar = (uint16_t)(((uint32_t)h.count[i] * ADC_HIST_BAR_MAX + peak - 1u) / peak);

        cyan();
        for (j = 0u; j < bar; j++){
            sendc('#');
        }
        standardColour();
        linebreak(1);
    }

    system();
    sends("min: ");
    sendNum(h.min);
    sends("  max: ");
    sendNum(h.max);
    sends("  below window: ");
    sendNum(h.below);
    sends("  above window: ");
    sendNum(h.above);
    linebreak(1);
}
