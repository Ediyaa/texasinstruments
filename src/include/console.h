#ifndef SRC_INCLUDE_CONSOLE_H_
#define SRC_INCLUDE_CONSOLE_H_

#define HIST_MAX  16
#define LINE_MAX  32

unsigned char checkSend(void);
unsigned char checkEmpfang(void);

void consoleUartInit(void);

void sendc(char c);
void sends(const char *s);

unsigned char rx_available(void);
void rx_flush(void);

void console_task(void);

void status(void);

void standardColour(void);

void grey(void);

void green(void);

void yellow(void);

void rot(void);

void blue(void);

void whitefat(void);

void cyan(void);

void cyanfat(void);


#endif /* SRC_INCLUDE_CONSOLE_H_ */
