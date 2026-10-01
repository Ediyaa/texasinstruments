#ifndef SRC_INCLUDE_CONSOLE_COMMANDS_H_
#define SRC_INCLUDE_CONSOLE_COMMANDS_H_

#include <stdbool.h>
#include <stdint.h>

/* adchist: Länge des längsten Balkens in Zeichen */
#define ADC_HIST_BAR_MAX  40u

/* adchist, adclphist: Nachkommastellen des Mittelwerts */
#define ADC_HIST_MEAN_DEC 4u

void commands(char *eingabe);

void listCommands(void);

/* true: main gibt jeden neuen ADC-Wert auf der Konsole aus */
extern volatile bool adcstreamflag;

/* Wert N in der mit adcunit_ gewählten Einheit ausgeben */
void adcPrint(uint16_t n);

#endif /* SRC_INCLUDE_CONSOLE_COMMANDS_H_ */
