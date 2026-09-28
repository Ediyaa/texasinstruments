#ifndef SRC_INCLUDE_CONSOLE_COMMANDS_H_
#define SRC_INCLUDE_CONSOLE_COMMANDS_H_

#include <stdbool.h>

void commands(char *eingabe);

void listCommands(void);

/* true: main gibt jeden neuen ADC-Wert auf der Konsole aus */
extern volatile bool adcstreamflag;

#endif /* SRC_INCLUDE_CONSOLE_COMMANDS_H_ */
