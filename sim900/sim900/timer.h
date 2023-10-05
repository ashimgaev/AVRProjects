#ifndef TIMER_H_
#define TIMER_H_

#define TIMER_NONE	0
#define HOURS	1
#define MINUTES	2

typedef unsigned char TimerType;
typedef void (*onTimer)(unsigned char t_cmd);

void TimerSetListener(TimerType type, unsigned char cnt, unsigned char cmd);
void TimerStart(onTimer l);
void TimerStop();

#endif /* TIMER_H_ */