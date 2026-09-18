#ifndef SRC_INCLUDE_TIMER_H_
#define SRC_INCLUDE_TIMER_H_

extern volatile int pwmtoggleflag;

void timerInitA0(void);

void timerSetDimmf(unsigned int wert);
void timersetDimm(unsigned int wert);

void pwmtoggle(void);

#endif /* SRC_INCLUDE_TIMER_H_ */
