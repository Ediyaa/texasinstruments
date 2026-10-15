#ifndef SRC_INCLUDE_CONSOLE_COMMANDS_H_
#define SRC_INCLUDE_CONSOLE_COMMANDS_H_

#include <stdbool.h>
#include <stdint.h>

/* adclphist: Länge des längsten Balkens in Zeichen */
#define ADC_HIST_BAR_MAX  40u

void commands(char *eingabe);

void listCommands(void);

/* true: main ruft fortlaufend adcStreamStep auf (Befehl adcstream) */
extern volatile bool adcstreamflag;

/* Ein Durchlauf von adcstream: Messung wie adclphist, Ausgabe eine Zeile */
void adcStreamStep(void);

#endif /* SRC_INCLUDE_CONSOLE_COMMANDS_H_ */
