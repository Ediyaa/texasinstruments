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
typedef void (*commandStrFn)(const char *);

typedef struct {
    const char  *name;
    commandFn    handler;     /* NULL = kein Aufruf, nur Ausgabe        */
    commandArgFn argHandler;  /* != NULL = Befehl erwartet eine Zahl    */
    const char  *argHint;     /* Hilfetext bei argHandler, z.B. "<zahl>"*/
    const char  *msg1;        /* NULL = keine Ausgabe                   */
    const char  *msg2;
    uint8_t      argFmt;      /* Format des Arguments, ARG_... unten.
                                 Fehlt der Eintrag, ist er ARG_UINT     */
    commandStrFn strHandler;  /* != NULL = Befehl liest sein Argument
                                 selbst, bekommt den Text nach dem Namen.
                                 Fehlt der Eintrag, ist er NULL         */
} command_t;

/* Formate für argFmt */
#define ARG_UINT    0u   /* ganze Zahl 0 ... 65535                          */
#define ARG_TENTHS  1u   /* eine Nachkommastelle, argHandler bekommt Wert * 10 */
#define ARG_SHUND   2u   /* Vorzeichen, bis zwei Nachkommastellen, argHandler
                            bekommt Wert * 100 als Zweierkomplement         */

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

static void adcStream(void);
static void adcLpRate(unsigned int wert);
static void adcLpBlocks(unsigned int wert);
static void adcLpHist(void);
static void adcLpTau(unsigned int wert);
static void adcVref(const char *arg);
static bool parseHundredthsAt(const char **ps, uint32_t maxInt, uint32_t *out);
static void adcVsrc(const char *arg);
static void adcR1(const char *arg);
static void adcOffset(unsigned int wert);
static void adcRound(unsigned int wert);
static void adcStatus(void);
static void adcPreset(void);
static void adcTlv(void);
static void adcVcc(void);
static void adcTlvSet(unsigned int wert);
static void adcNtc(const char *arg);

volatile bool adcstreamflag = false;

/* Gesamtdurchschnitt von adcstream: Summe und Anzahl aller gefilterten Werte
   seit dem Einschalten, 64 Bit gegen Überlauf bei langem Lauf */
static uint64_t s_stream_sum = 0ULL;
static uint64_t s_stream_n   = 0ULL;


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
    { "adclphist",   adcLpHist, NULL, NULL, NULL, NULL },
    { "adcstream",   adcStream, NULL, NULL, NULL, NULL },
    { "adclprate_",  NULL,      adcLpRate,   "<INTEGER[1,3]> 1=1kHz 2=10kHz 3=100kHz", NULL, NULL },
    { "adclptau_",   NULL,      adcLpTau,    "<INTEGER[1,20]> tau = 2^K samples", NULL, NULL },
    { "adclpblocks_", NULL,     adcLpBlocks, "<INTEGER[1,31]> blocks of 2048 samples per run", NULL, NULL },
    { "adcvref_",    NULL,      NULL,        "<DECIMAL[0.01,6553.50]> V_R+ in mV, e.g. 2493.85", NULL, NULL, ARG_UINT, adcVref },
    { "adcvsrc_",    NULL,      NULL,        "<DECIMAL[0.01,6553.50]> source voltage in mV, e.g. 3306.12", NULL, NULL, ARG_UINT, adcVsrc },
    { "adcr1_",      NULL,      NULL,        "<DECIMAL[0.1,65535.0]> R_1 in Ohm, e.g. 1001.6", NULL, NULL, ARG_UINT, adcR1 },
    { "adcoffset_",  NULL,      adcOffset,   "<DECIMAL[-327.67,327.67]> ADC offset in LSB, e.g. -0.59", NULL, NULL, ARG_SHUND },
    { "adcround_",   NULL,      adcRound,    "<INTEGER[0,4]> decimals of N before U, R, T (4 = no rounding)", NULL, NULL },
    { "adcntc_",     NULL,      NULL,        "<1..3>_<T in degC>_<R_T in Ohm>, e.g. adcntc_1_0.00_31687.00", NULL, NULL, ARG_UINT, adcNtc },
    { "adcpreset",   adcPreset, NULL, NULL, NULL, NULL },
    { "adctlv",      adcTlv,    NULL, NULL, NULL, NULL },
    { "adcvcc",      adcVcc,    NULL, NULL, NULL, NULL },
    { "adctlvset_",  NULL,      adcTlvSet,   "<INTEGER[0,1]> factory cal -> adcvref_, adcoffset_ (1 = with gain factor)", NULL, NULL },
    { "adcstatus",   adcStatus, NULL, NULL, NULL, NULL },
    { NULL,          NULL,      NULL, NULL, NULL, NULL }
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

/* Zahl mit höchstens einer Nachkommastelle einlesen, Ergebnis * 10:
 * "2494" -> 24940, "2493.8" oder "2493,8" -> 24938.
 * Liefert false bei leerer Eingabe, anderen Zeichen, mehr als einer
 * Nachkommastelle oder Ergebnis über 65535 (6553,5). */
static bool parseTenths(const char *s, unsigned int *out){

    unsigned long wert = 0UL;
    bool ziffern = false;

    for ( ; *s >= '0' && *s <= '9' ; s++){
        wert = wert * 10UL + (unsigned long)(*s - '0');
        ziffern = true;
        if (wert > 6553UL) return false;          /* * 10 läge über 65535 */
    }
    if (!ziffern) return false;

    wert *= 10UL;

    if (*s == '.' || *s == ','){
        s++;
        if (*s < '0' || *s > '9') return false;
        wert += (unsigned long)(*s - '0');
        s++;
    }

    if (*s != '\0') return false;
    if (wert > 65535UL) return false;

    *out = (unsigned int)wert;
    return true;
}

/* Zahl mit Vorzeichen und höchstens zwei Nachkommastellen einlesen,
 * Ergebnis * 100 als Zweierkomplement: "-0.59" -> -59, "2.3" -> 230, "+1" -> 100.
 * Liefert false bei leerer Eingabe, anderen Zeichen, mehr als zwei
 * Nachkommastellen oder Betrag über 327,67. */
static bool parseSignedHundredths(const char *s, unsigned int *out){

    unsigned long betrag = 0UL;
    bool negativ = false;
    bool ziffern = false;
    int  stellen = 0;

    if (*s == '-' || *s == '+'){
        negativ = (*s == '-');
        s++;
    }

    for ( ; *s >= '0' && *s <= '9' ; s++){
        betrag = betrag * 10UL + (unsigned long)(*s - '0');
        ziffern = true;
        if (betrag > 327UL) return false;         /* * 100 läge über 32767 */
    }
    if (!ziffern) return false;

    betrag *= 100UL;

    if (*s == '.' || *s == ','){
        s++;
        if (*s >= '0' && *s <= '9'){
            betrag += 10UL * (unsigned long)(*s - '0');
            s++;
            stellen++;
        }
        if (*s >= '0' && *s <= '9'){
            betrag += (unsigned long)(*s - '0');
            s++;
            stellen++;
        }
        if (stellen == 0) return false;
    }

    if (*s != '\0') return false;
    if (betrag > 32767UL) return false;

    *out = negativ ? (unsigned int)(-(int)betrag) : (unsigned int)betrag;
    return true;
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

            if (cmd[i].strHandler != NULL){

                /* Befehl liest sein Argument selbst */
                size_t len = strlen(cmd[i].name);

                if (strncmp(eingabe, cmd[i].name, len) == 0){
                    cmd[i].strHandler(&eingabe[len]);
                    return;
                }
            }
            else if (cmd[i].argHandler != NULL){

                /* Befehl mit Argument: Praefix vergleichen, Rest als Zahl lesen. */
                size_t len = strlen(cmd[i].name);

                if (strncmp(eingabe, cmd[i].name, len) == 0){

                    unsigned int wert;
                    bool ok;

                    if (cmd[i].argFmt == ARG_TENTHS){
                        ok = parseTenths(&eingabe[len], &wert);
                    }
                    else if (cmd[i].argFmt == ARG_SHUND){
                        ok = parseSignedHundredths(&eingabe[len], &wert);
                    }
                    else {
                        ok = parseUInt(&eingabe[len], &wert);
                    }

                    if (ok){
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

/* 64-Bit-Zahl ausgeben (höchstens 20 Ziffern) */
static void sendNumLL(uint64_t n){
    char buf[21];
    int  i = 20;

    buf[i] = '\0';
    do {
        buf[--i] = (char)('0' + (int)(n % 10ULL));
        n /= 10ULL;
    } while (n != 0ULL);

    sends(&buf[i]);
}

/* Festkommazahl q mit dec Nachkommastellen ausgeben (q = Wert * 10^dec) */
static void sendFixed(uint64_t q, uint16_t dec){
    uint64_t scale = 1ULL;
    uint64_t t;
    uint64_t frac;
    uint16_t d;

    for (d = 0u; d < dec; d++){
        scale *= 10ULL;
    }

    frac = q % scale;
    sendNumLL(q / scale);
    if (dec == 0u){
        return;
    }
    sendc('.');
    for (t = scale / 10ULL; t > 0ULL; t /= 10ULL){      /* genau dec Ziffern */
        sendc((char)('0' + (int)((frac / t) % 10ULL)));
    }
}

/* Festkommazahl mit Vorzeichen ausgeben, q = Wert * 10^dec */
static void sendSignedFixed(int32_t q, uint16_t dec){
    if (q < 0L){
        sendc('-');
        sendFixed((uint64_t)(-(int64_t)q), dec);
    }
    else {
        sendFixed((uint64_t)q, dec);
    }
}

/* Temperatur in 0,001 °C ausgeben, mit Einheit */
static void sendMilliCelsius(int32_t t){
    if (t == ADC_TEMP_INVALID){
        sends("invalid");
        return;
    }
    if (t == ADC_TEMP_RANGE){
        sends("out of range");
        return;
    }
    sendSignedFixed(t, 3u);
    sends(" \xc2\xb0""C");               /* UTF-8 Gradzeichen */
}

/* Fehlermeldung, wenn die Temperatur außerhalb ADC_T_MIN_MC ... ADC_T_MAX_MC
   liegt oder R_T nicht bestimmbar ist (ohne Zeilenumbruch) */
static void tempRangeErrorPrint(void){
    red();
    sends("ERROR: temperature outside ");
    sendSignedFixed(ADC_T_MIN_MC / 1000L, 0u);
    sends(" ... ");
    sendSignedFixed(ADC_T_MAX_MC / 1000L, 0u);
    sends(" \xc2\xb0""C");
    standardColour();
}

/* Eine Zeile Mittelwert: N, Spannung, Widerstand und Temperatur */
static void meanPrint(const char *label, uint32_t sum, uint32_t n){
    uint32_t nq;
    uint64_t rq;

    system();
    sends(label);

    if (n == 0UL){
        sends("-");
        linebreak(1);
        return;
    }

    nq = adcMeanFixed(sum, n);

    if (!adcFixedTempOk(nq)){
        tempRangeErrorPrint();
        linebreak(1);
        return;
    }

    cyan();
    sendFixed(nq, ADC_MEAN_DEC);
    standardColour();

    sends("  U: ");
    cyan();
    sendFixed(adcFixedToMillivolt(nq), ADC_MEAN_DEC);
    sends(" mV");
    standardColour();

    sends("  R: ");
    cyan();
    if (adcFixedToOhm(nq, &rq)){
        sendFixed(rq, ADC_OHM_DEC);
        sends(" Ohm");
        standardColour();

        sends("  T: ");
        cyan();
        sendMilliCelsius(adcFixedOhmToMilliCelsius(rq));
    }
    else {
        sends("invalid");
    }
    standardColour();
    linebreak(1);
}

/* Balken, min, max und Werte außerhalb des Fensters ausgeben */
static void histPrint(const adcHist_t *h){
    uint16_t i;
    uint16_t j;
    uint16_t first = ADC_HIST_BINS;      /* erste belegte Klasse    */
    uint16_t last  = 0u;                  /* letzte belegte Klasse   */
    uint16_t peak  = 0u;                  /* größte Anzahl je Klasse */
    uint16_t bar;

    for (i = 0u; i < ADC_HIST_BINS; i++){
        if (h->count[i] != 0u){
            if (first == ADC_HIST_BINS) first = i;
            last = i;
            if (h->count[i] > peak) peak = h->count[i];
        }
    }

    /* Nur von der ersten bis zur letzten belegten Klasse. Liegen alle
       Ergebnisse außerhalb des Fensters, ist first > last und die
       Schleife läuft nicht (peak = 0 wird dann nicht als Teiler benutzt) */
    for (i = first; i <= last; i++){
        sendNumPad((unsigned int)(h->low + i), 4u);
        sends(" |");
        sendNumPad(h->count[i], 5u);
        sendc(' ');

        /* aufgerundet, damit jede belegte Klasse mindestens ein Zeichen hat */
        bar = (uint16_t)(((uint32_t)h->count[i] * ADC_HIST_BAR_MAX + peak - 1u) / peak);

        cyan();
        for (j = 0u; j < bar; j++){
            sendc('#');
        }
        standardColour();
        linebreak(1);
    }

    system();
    sends("min: ");
    sendNum(h->min);
    sends("  max: ");
    sendNum(h->max);
    sends("  below window: ");
    sendNum(h->below);
    sends("  above window: ");
    sendNum(h->above);
    linebreak(1);

    /* Mittelwerte der gezählten Werte und der zugehörigen Rohwerte,
       jeweils mit Spannung und Widerstand */
    meanPrint("mean:     ", h->sum, h->n);
    meanPrint("raw mean: ", h->sum_raw, h->n);
}


/* Abtastrate der Blockmessung wählen */
static void adcLpRate(unsigned int wert){
    if (!adcBlockRateSet((uint16_t)wert)){
        system();
        sends("invalid argument for: ");
        red();
        sends("adclprate_");
        linebreak(1);
        return;
    }
    system();
    sends("lowpass block rate set to: ");
    cyan();
    sendNumL(adcBlockRateHz());
    sends(" Hz");
    standardColour();
    linebreak(1);
}

/* Zeitkonstante des Blocktiefpasses ausgeben: "2^K = N samples = T us"
   (T bei der eingestellten Rate, gerundet) */
static void lpTauPrint(void){
    uint16_t k   = adcLpShiftGet();
    uint32_t n   = 1UL << k;
    uint32_t f   = adcBlockRateHz();
    /* 64 Bit: n * 10^6 läuft ab K = 13 über 32 Bit */
    uint32_t tus = (uint32_t)(((uint64_t)n * 1000000ULL + f / 2UL) / f);

    sends("2^");
    sendNum(k);
    sends(" = ");
    sendNumL(n);
    sends(" samples = ");
    sendNumL(tus);
    sends(" us");
}

/* Zeitkonstante des Blocktiefpasses einstellen: K = wert, tau = 2^K Werte */
static void adcLpTau(unsigned int wert){
    if (!adcLpShiftSet((uint16_t)wert)){
        system();
        sends("invalid argument for: ");
        red();
        sends("adclptau_");
        linebreak(1);
        return;
    }
    system();
    sends("lowpass time constant set to: ");
    cyan();
    lpTauPrint();
    standardColour();
    linebreak(1);
}

/* Blöcke aufnehmen, Tiefpass über jeden Block, Histogramm der gefilterten Werte.
   Ausgabe erst nach dem letzten Block */
static void adcLpHist(void){
    static adcHist_t h;                   /* nicht auf dem Stack */

    /* Bit UCBUSY: warten, bis die letzten Zeichen gesendet sind */
    while (UCA1STAT & UCBUSY){
    }

    /* Jedes Histogramm startet den Tiefpass neu, damit ein veralteter
       Zustand aus einer früheren Messung das Ergebnis nicht verfälscht */
    adcLpReset();

    if (!adcBlockLowpassHistogram(&h)){
        system();
        red();
        sends("ADC block capture failed (timeout)");
        standardColour();
        linebreak(1);
        return;
    }

    /* Temperatur des Mittelwerts außerhalb des Bereichs: nur Fehler, kein Histogramm */
    if (!adcFixedTempOk(adcMeanFixed(h.sum, h.n))){
        system();
        tempRangeErrorPrint();
        linebreak(1);
        return;
    }

    system();
    sends("ADC lowpass block histogram, rate: ");
    cyan();
    sendNumL(adcBlockRateHz());
    sends(" Hz");
    standardColour();
    sends(", samples: ");
    cyan();
    sendNumL(h.n);
    standardColour();
    sends(", tau: ");
    cyan();
    lpTauPrint();
    standardColour();
    linebreak(1);

    histPrint(&h);
}

/* Umrechnungsparameter: Rückmeldung bzw. Fehlermeldung */
static void paramSetReply(bool ok, const char *cmd, const char *what,
                          unsigned int wert, uint16_t dec, const char *unit){
    system();
    if (!ok){
        sends("invalid argument for: ");
        red();
        sends(cmd);
        linebreak(1);
        return;
    }
    sends(what);
    sends(" set to: ");
    cyan();
    sendFixed(wert, dec);                 /* dec = 1: wert in Zehnteln */
    sends(unit);
    standardColour();
    linebreak(1);
}

/* Spannung in mV mit höchstens zwei Nachkommastellen einlesen und setzen,
   Rückmeldung wie paramSetReply. set bekommt den Wert in 0,01 mV */
static void mvSetReply(const char *arg, bool (*set)(uint32_t),
                       const char *cmd, const char *what){
    const char *s = arg;
    uint32_t    v;

    system();
    if (!parseHundredthsAt(&s, ADC_MV_MAX_X100 / 100UL, &v) || *s != '\0' || !set(v)){
        sends("invalid argument for: ");
        red();
        sends(cmd);
        linebreak(1);
        return;
    }
    sends(what);
    sends(" set to: ");
    cyan();
    sendFixed(v, 2u);
    sends(" mV");
    standardColour();
    linebreak(1);
}

/* V_R+ für die Umrechnung in mV (ändert nicht die Referenz selbst) */
static void adcVref(const char *arg){
    mvSetReply(arg, adcVrefSet, "adcvref_", "V_R+");
}

/* Spannung der Signalquelle (Versorgung des Teilers) in mV */
static void adcVsrc(const char *arg){
    mvSetReply(arg, adcVsrcSet, "adcvsrc_", "source voltage");
}

/* Zahl mit höchstens einer Nachkommastelle einlesen, Ergebnis * 10 in 32 Bit:
   "1002" -> 10020, "1001.6" oder "1001,6" -> 10016.
   Liefert false bei leerer Eingabe, anderen Zeichen, mehr als einer
   Nachkommastelle oder Ergebnis über max_x10 */
static bool parseTenthsL(const char *s, uint32_t max_x10, uint32_t *out){
    uint32_t wert = 0UL;
    bool     ziffern = false;

    for ( ; *s >= '0' && *s <= '9' ; s++){
        wert = wert * 10UL + (uint32_t)(*s - '0');
        ziffern = true;
        if (wert > max_x10 / 10UL) return false;
    }
    if (!ziffern) return false;

    wert *= 10UL;

    if (*s == '.' || *s == ','){
        s++;
        if (*s < '0' || *s > '9') return false;
        wert += (uint32_t)(*s - '0');
        s++;
    }

    if (*s != '\0') return false;
    if (wert > max_x10) return false;

    *out = wert;
    return true;
}

/* Festwiderstand R_1 des Spannungsteilers in Ohm, eine Nachkommastelle */
static void adcR1(const char *arg){
    uint32_t r;

    system();
    if (!parseTenthsL(arg, ADC_R1_MAX_X10, &r) || !adcR1Set(r)){
        sends("invalid argument for: ");
        red();
        sends("adcr1_");
        linebreak(1);
        return;
    }
    sends("R_1 set to: ");
    cyan();
    sendFixed(r, 1u);
    sends(" Ohm");
    standardColour();
    linebreak(1);
}

/* Offset o des ADC in Stufen, wert = o * 100 als Zweierkomplement */
static void adcOffset(unsigned int wert){
    int16_t o = (int16_t)wert;

    adcOffsetSet(o);

    system();
    sends("ADC offset set to: ");
    cyan();
    if (o < 0){
        sendc('-');
    }
    sendFixed((uint64_t)((o < 0) ? -(int32_t)o : (int32_t)o), 2u);
    sends(" LSB");
    standardColour();
    linebreak(1);
}

/* Offset in Stufen ausgeben, o in 0,01 Stufen mit Vorzeichen */
static void offsetPrint(int16_t o){
    if (o < 0){
        sendc('-');
    }
    sendFixed((uint64_t)((o < 0) ? -(int32_t)o : (int32_t)o), 2u);
    sends(" LSB");
}

/* Eine Zeile der Statusausgabe beginnen: "System: <label>" und Farbe cyan */
static void statusLabel(const char *label){
    system();
    sends(label);
    cyan();
}

/* Eine Zeile der Statusausgabe abschließen */
static void statusEnd(void){
    standardColour();
    linebreak(1);
}

/* float in wissenschaftlicher Schreibweise mit 7 gültigen Ziffern ausgeben,
   z. B. 1.137292e-03 */
static void sendSci(float f){
    double   v = (double)f;
    int16_t  e = 0;
    uint32_t m;

    if (v < 0.0){
        sendc('-');
        v = -v;
    }
    if (v != 0.0){
        while (v >= 10.0){ v /= 10.0; e++; }
        while (v <  1.0) { v *= 10.0; e--; }
    }

    m = (uint32_t)(v * 1000000.0 + 0.5);        /* 1 000 000 ... 10 000 000 */
    if (m >= 10000000UL){
        m /= 10UL;
        e++;
    }
    sendFixed(m, 6u);

    sends("e");
    if (e < 0){
        sendc('-');
        e = (int16_t)-e;
    }
    else {
        sendc('+');
    }
    if (e < 10){
        sendc('0');
    }
    sendNum((unsigned int)e);
}

/* Kennlinie: Herkunft, Stützpunkte und Koeffizienten ausgeben */
static void ntcStatus(void){
    uint16_t i;
    int32_t  t;
    uint32_t r;
    float    a, b, c;

    statusLabel("NTC curve (adcntc_):         ");
    sends(adcNtcFitted() ? "fitted from points" : "table (default)");
    statusEnd();

    for (i = 0u; i < ADC_NTC_POINTS; i++){
        system();
        sends("  point ");
        sendNum(i + 1u);
        sends(":                    ");
        cyan();
        if (adcNtcPointGet(i, &t, &r)){
            sendSignedFixed(t, 2u);
            sends(" \xc2\xb0""C, ");
            sendFixed(r, 2u);
            sends(" Ohm");
        }
        else {
            sends("-");
        }
        statusEnd();
    }

    adcNtcCoeffGet(&a, &b, &c);
    statusLabel("  A, B, C:                   ");
    sendSci(a);
    sends(", ");
    sendSci(b);
    sends(", ");
    sendSci(c);
    statusEnd();
}

/* Alle aktuellen Einstellungen des ADC ausgeben */
static void adcStatus(void){

    statusLabel("V_R+ (adcvref_):             ");
    sendFixed(adcVrefGet(), 2u);
    sends(" mV");
    statusEnd();

    statusLabel("source voltage (adcvsrc_):   ");
    sendFixed(adcVsrcGet(), 2u);
    sends(" mV");
    statusEnd();

    statusLabel("R_1 (adcr1_):                ");
    sendFixed(adcR1Get(), 1u);
    sends(" Ohm");
    statusEnd();

    statusLabel("offset (adcoffset_):         ");
    offsetPrint(adcOffsetGet());
    statusEnd();

    statusLabel("N decimals (adcround_):      ");
    sendNum(adcRoundGet());
    if (adcRoundGet() == ADC_MEAN_DEC){
        sends(" (no rounding)");
    }
    statusEnd();

    statusLabel("block rate (adclprate_):     ");
    sendNum(adcBlockRateGet());
    sends(" = ");
    sendNumL(adcBlockRateHz());
    sends(" Hz");
    statusEnd();

    statusLabel("lowpass tau (adclptau_):     ");
    lpTauPrint();
    statusEnd();

    statusLabel("blocks (adclpblocks_):       ");
    sendNum(adcLpBlocksGet());
    sends(" x 2048 = ");
    sendNumL((unsigned long)adcLpBlocksGet() * ADC_BLOCK_LEN);
    sends(" samples");
    statusEnd();

    statusLabel("stream (adcstream):          ");
    sends(adcstreamflag ? "ON" : "OFF");
    statusEnd();

    ntcStatus();
}

/* Laufzeitparameter auf die Kalibrierwerte ADC_PRESET_... (adc.h) setzen.
   Kennlinie (adcntc_) und Stream bleiben unverändert */
/* Werkskalibrierung aus dem TLV ausgeben (SLAS590P Tab. 9-62, SLAU208Q 1.13.5).
   Faktoren roh und umgerechnet: Faktor / 2^15 mit 5 Stellen, Referenz in mV */
static void adcTlv(void){
    static const char *const refName[3] = { "1.5 V", "2.0 V", "2.5 V" };
    static const uint32_t refNomX100[3] = { 150000UL, 200000UL, 250000UL };  /* 0,01 mV */
    adcTlv_t t;
    uint16_t i;

    adcTlvRead(&t);

    statusLabel("ADC12 cal tag (01A14h):      ");
    sends(t.adc_valid ? "OK" : "invalid");
    statusEnd();

    statusLabel("gain factor (01A16h):        ");
    sendNum(t.gain);
    sends(" = ");
    sendFixed(((uint64_t)t.gain * 100000ULL + 16384ULL) / 32768ULL, 5u);
    sends(" (1/GAIN)");
    statusEnd();

    statusLabel("offset (01A18h):             ");
    sendSignedFixed(t.offset, 0u);
    sends(" LSB (added)");
    statusEnd();

    for (i = 0u; i < 3u; i++){
        statusLabel("temp sensor ");
        sends(refName[i]);
        sends(" ref:      30 C = ");
        sendNum(t.t30[i]);
        sends(", 85 C = ");
        sendNum(t.t85[i]);
        statusEnd();
    }

    statusLabel("REF cal tag (01A26h):        ");
    sends(t.ref_valid ? "OK" : "invalid");
    statusEnd();

    for (i = 0u; i < 3u; i++){
        statusLabel("REF ");
        sends(refName[i]);
        sends(" factor (01A");
        sends(i == 0u ? "28" : (i == 1u ? "2A" : "2C"));
        sends("h):  ");
        sendNum(t.ref[i]);
        sends(" -> ");
        sendFixed(((uint64_t)refNomX100[i] * t.ref[i] + 16384ULL) / 32768ULL, 2u);
        sends(" mV");
        statusEnd();
    }

    /* AV_CC über den internen Teiler an Kanal 11 (nicht werkskalibriert) */
    adcVcc();
}

/* AV_CC über den internen Teiler an Kanal 11 messen und ausgeben (Befehl adcvcc,
   auch am Ende von adctlv). Messung über adcVccMeasure: 100 kHz, tau = 2^12,
   10 x 2048 Werte, ca. 205 ms. AV_CC = 2 * U mit adcvref_ und adcoffset_ */
static void adcVcc(void){
    uint32_t nq;

    /* Bit UCBUSY: warten, bis die letzten Zeichen gesendet sind */
    while (UCA1STAT & UCBUSY){
    }

    statusLabel("AV_CC via ch 11 (/2):        ");
    if (!adcVccMeasure(&nq)){
        red();
        sends("ADC block capture failed (timeout)");
        statusEnd();
        return;
    }
    sends("N = ");
    sendFixed(nq, ADC_MEAN_DEC);
    sends(" -> AV_CC = ");
    sendFixed(2ULL * adcFixedToMillivolt(nq), ADC_MEAN_DEC);
    sends(" mV");
    statusEnd();

    statusLabel("  (divider 0.48 ... 0.52, not factory calibrated; 100 kHz, tau 2^12, 10 x 2048)");
    statusEnd();
}

/* Werkskalibrierung in adcvref_ und adcoffset_ übernehmen.
   wert = 0: Referenzfaktor und Offset, wert = 1: zusätzlich Verstärkungsfaktor */
static void adcTlvSet(unsigned int wert){
    if (wert > 1u){
        system();
        sends("invalid argument for: ");
        red();
        sends("adctlvset_");
        linebreak(1);
        return;
    }
    if (!adcTlvApply(wert == 1u)){
        system();
        red();
        sends("adctlvset: factory calibration in TLV invalid, nothing changed");
        standardColour();
        linebreak(1);
        return;
    }

    statusLabel("V_R+ (adcvref_):             ");
    sendFixed(adcVrefGet(), 2u);
    sends(" mV");
    statusEnd();

    statusLabel("offset (adcoffset_):         ");
    offsetPrint(adcOffsetGet());
    statusEnd();

    statusLabel("gain factor:                 ");
    sends(wert == 1u ? "included in V_R+" : "not used");
    statusEnd();
}

static void adcPreset(void){
    bool ok = true;

    ok &= adcVrefSet(ADC_PRESET_VREF_X100);
    ok &= adcVsrcSet(ADC_PRESET_VSRC_X100);
    ok &= adcR1Set(ADC_PRESET_R1_X10);
    adcOffsetSet(ADC_PRESET_OFFSET_X100);
    ok &= adcRoundSet(ADC_PRESET_ROUND_DEC);
    ok &= adcBlockRateSet(ADC_PRESET_BLK_RATE);
    ok &= adcLpShiftSet(ADC_PRESET_LP_SHIFT);
    ok &= adcLpBlocksSet(ADC_PRESET_LP_BLOCKS);

    system();
    if (!ok){
        red();
        sends("adcpreset: invalid value in ADC_PRESET_... (adc.h)");
        standardColour();
        linebreak(1);
    }
    else {
        sends("ADC calibration values set:");
        linebreak(1);
    }

    adcStatus();
}

/* Ziffern mit höchstens zwei Nachkommastellen ab *ps einlesen, Ergebnis * 100.
   Bricht beim ersten anderen Zeichen ab und lässt *ps darauf zeigen.
   Liefert false ohne Ziffer vor dem Komma, ohne Ziffer nach dem Komma
   oder wenn der ganzzahlige Teil über maxInt liegt */
static bool parseHundredthsAt(const char **ps, uint32_t maxInt, uint32_t *out){
    const char *s = *ps;
    uint32_t wert = 0UL;
    bool     ziffern = false;
    uint16_t stellen = 0u;

    for ( ; *s >= '0' && *s <= '9' ; s++){
        wert = wert * 10UL + (uint32_t)(*s - '0');
        ziffern = true;
        if (wert > maxInt) return false;
    }
    if (!ziffern) return false;

    wert *= 100UL;

    if (*s == '.' || *s == ','){
        s++;
        if (*s >= '0' && *s <= '9'){
            wert += 10UL * (uint32_t)(*s - '0');
            s++;
            stellen++;
        }
        if (*s >= '0' && *s <= '9'){
            wert += (uint32_t)(*s - '0');
            s++;
            stellen++;
        }
        if (stellen == 0u) return false;
    }

    *ps  = s;
    *out = wert;
    return true;
}

/* Stützpunkt der Kennlinie setzen: adcntc_<i>_<T>_<R_T>
   i = 1 ... 3, T in °C (Vorzeichen, bis zwei Nachkommastellen),
   R_T in Ohm (bis zwei Nachkommastellen) */
static void adcNtc(const char *arg){
    const char *s = arg;
    uint16_t idx;
    uint32_t t_abs;
    uint32_t r;
    int32_t  t;
    bool     neg = false;
    uint16_t res;

    /* Index */
    if (*s < '1' || *s > (char)('0' + ADC_NTC_POINTS)) goto invalid;
    idx = (uint16_t)(*s - '1');
    s++;
    if (*s != '_') goto invalid;
    s++;

    /* Temperatur */
    if (*s == '-' || *s == '+'){
        neg = (*s == '-');
        s++;
    }
    if (!parseHundredthsAt(&s, 999UL, &t_abs)) goto invalid;
    t = neg ? -(int32_t)t_abs : (int32_t)t_abs;
    if (*s != '_') goto invalid;
    s++;

    /* Widerstand */
    if (!parseHundredthsAt(&s, 9999999UL, &r)) goto invalid;
    if (*s != '\0') goto invalid;

    res = adcNtcPointSet(idx, t, r);
    if (res == ADC_NTC_BADARG) goto invalid;

    system();
    sends("NTC point ");
    sendNum(idx + 1u);
    sends(" set to: ");
    cyan();
    sendSignedFixed(t, 2u);
    sends(" \xc2\xb0""C, ");
    sendFixed(r, 2u);
    sends(" Ohm");
    standardColour();
    linebreak(1);

    system();
    if (res == ADC_NTC_FITTED){
        sends("NTC curve fitted and active");
        linebreak(1);
        ntcStatus();
    }
    else if (res == ADC_NTC_BADFIT){
        red();
        sends("points give no falling NTC curve, previous curve stays active");
        standardColour();
        linebreak(1);
    }
    else {
        sends("set all 3 points to fit the curve");
        linebreak(1);
    }
    return;

invalid:
    system();
    sends("invalid argument for: ");
    red();
    sends("adcntc_");
    standardColour();
    linebreak(1);
}

/* Nachkommastellen, auf die N vor U, R und T gerundet wird */
static void adcRound(unsigned int wert){
    if (!adcRoundSet((uint16_t)wert)){
        system();
        sends("invalid argument for: ");
        red();
        sends("adcround_");
        linebreak(1);
        return;
    }
    system();
    sends("N rounded to: ");
    cyan();
    sendNum(wert);
    sends(" decimals");
    standardColour();
    if (wert == ADC_MEAN_DEC){
        sends(" (no rounding)");
    }
    linebreak(1);
}

/* Anzahl Blöcke je Durchlauf von adclphist und adcstream */
static void adcLpBlocks(unsigned int wert){
    if (!adcLpBlocksSet((uint16_t)wert)){
        system();
        sends("invalid argument for: ");
        red();
        sends("adclpblocks_");
        linebreak(1);
        return;
    }
    system();
    sends("lowpass blocks set to: ");
    cyan();
    sendNum(wert);
    sends(" x 2048 = ");
    sendNumL((unsigned long)wert * ADC_BLOCK_LEN);
    sends(" samples");
    standardColour();
    linebreak(1);
}

/* adcstream ein- bzw. ausschalten. Eingeschaltet ruft main fortlaufend
   adcStreamStep auf: ein Durchlauf wie adclphist, Ausgabe eine Zeile */
static void adcStream(void){
    adcstreamflag = !adcstreamflag;
    system();
    if (adcstreamflag){
        adcLpReset();                     /* Tiefpass neu starten, danach läuft er durch */
        s_stream_sum = 0ULL;              /* Gesamtdurchschnitt neu beginnen */
        s_stream_n   = 0ULL;
        sends("ADC stream - ON, rate: ");
        cyan();
        sendNumL(adcBlockRateHz());
        sends(" Hz");
        standardColour();
        sends(", blocks: ");
        cyan();
        sendNum(adcLpBlocksGet());
        standardColour();
        sends(", tau: ");
        cyan();
        lpTauPrint();
        standardColour();
    }
    else {
        sends("ADC stream - OFF");
    }
    linebreak(1);
}

/* Ein Durchlauf von adcstream: Blöcke aufnehmen und filtern wie adclphist,
   dann eine Zeile mit Mittelwert N, U, R, T und min/max ausgeben */
/* N, U, R und T zu einem Festkomma-N ausgeben (eine Zeile von adcstream, ohne Umbruch) */
static void streamValuesPrint(uint32_t nq){
    uint64_t rq;

    sends("N: ");
    cyan();
    sendFixed(nq, ADC_MEAN_DEC);
    standardColour();

    sends("  U: ");
    cyan();
    sendFixed(adcFixedToMillivolt(nq), ADC_MEAN_DEC);
    sends(" mV");
    standardColour();

    sends("  R: ");
    cyan();
    if (adcFixedToOhm(nq, &rq)){
        sendFixed(rq, ADC_OHM_DEC);
        sends(" Ohm");
        standardColour();
        sends("  T: ");
        cyan();
        sendMilliCelsius(adcFixedOhmToMilliCelsius(rq));
    }
    else {
        sends("invalid");
    }
    standardColour();
}

void adcStreamStep(void){
    static adcHist_t h;                   /* nicht auf dem Stack */

    /* Bit UCBUSY: warten, bis die letzten Zeichen gesendet sind */
    while (UCA1STAT & UCBUSY){
    }

    if (!adcBlockLowpassHistogram(&h)){
        adcstreamflag = false;
        system();
        red();
        sends("ADC block capture failed (timeout), stream - OFF");
        standardColour();
        linebreak(1);
        return;
    }

    /* Zeile 1: dieser Durchlauf. Außerhalb des Temperaturbereichs nur der Fehler,
       und der Durchlauf zählt nicht zum Gesamtdurchschnitt */
    if (!adcFixedTempOk(adcMeanFixed(h.sum, h.n))){
        tempRangeErrorPrint();
        linebreak(1);
    }
    else {
        s_stream_sum += h.sum;
        s_stream_n   += h.n;

        streamValuesPrint(adcMeanFixed(h.sum, h.n));
        sends("  min/max: ");
        sendNum(h.min);
        sendc('/');
        sendNum(h.max);
        linebreak(1);
    }

    /* Zeile 2: Durchschnitt über alle gültigen Werte seit dem Einschalten von adcstream */
    sends("avg ");
    if (s_stream_n == 0ULL){
        sends("-");
    }
    else if (!adcFixedTempOk(adcMeanFixed(s_stream_sum, s_stream_n))){
        tempRangeErrorPrint();
    }
    else {
        streamValuesPrint(adcMeanFixed(s_stream_sum, s_stream_n));
        sends("  samples: ");
        sendFixed(s_stream_n, 0u);
    }
    linebreak(1);
}
