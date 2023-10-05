#include "defines.h"
#include <avr/interrupt.h>
#include "timer.h"

#ifndef F_CPU
#error "F_CPU not declared!"
#endif

#ifdef _DEBUG_
	#define SECONDS_IN_MINUTE	3
	#define MINUTES_IN_HOUR	3
#else
	#define SECONDS_IN_MINUTE	60
	#define MINUTES_IN_HOUR	60
#endif

#define SET_TIMER1_PRESCALER_0() TCCR1B=0
#define SET_TIMER1_PRESCALER_1() TCCR1B=(1<<CS10) // 1
#define SET_TIMER1_PRESCALER_8() TCCR1B=(1<<CS11) // 8
#define SET_TIMER1_PRESCALER_64() TCCR1B=(1<<CS11)|(1<<CS10) // 64
#define SET_TIMER1_PRESCALER_256() TCCR1B=(1<<CS12) // 256
#define SET_TIMER1_PRESCALER_1024() TCCR1B=(1<<CS12)|(1<<CS10) // 1024

#if defined(_DEBUG_)
	#define SET_TIMER1_PRESCALER() SET_TIMER1_PRESCALER_1()
	#define OVF_SECONDS 1
	#define CORRECTOR 0
#elif F_CPU==10000000UL // 10 Mgz
	#define SET_TIMER1_PRESCALER() SET_TIMER1_PRESCALER_1024()
	#define OVF_SECONDS 6 // 6.7 sec for 1 overflow for 10Mhz for prescaler 1024
	#define CORRECTOR 7
#elif F_CPU==1000000UL // 1Mgz
	#define SET_TIMER1_PRESCALER() SET_TIMER1_PRESCALER_64()
	#define OVF_SECONDS 4 // 4.2 sec for 1 overflow for 1Mhz for prescaler 64
	#define CORRECTOR 2
#else
	#error "Bad F_CPU !!!"
#endif

#define ENABLE_TIMER1_OVF_INTERRUPT() TIMSK |= (1<<TOIE1);
#define DISABLE_TIMER1_OVF_INTERRUPT() TIMSK&=~(1<<TOIE1)

#define DISABLE_TIMER1_INTERRUPTS() TIMSK=0

volatile unsigned char totalSecCorrector; // нужен т.к. TIMER1_OVF_vect не кратен 10
volatile unsigned char totalSec;
volatile unsigned char totalMin;
volatile unsigned char totalHour;
volatile TimerType type;
volatile unsigned char tickCnt;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      
volatile unsigned char cmd;

onTimer l;

ISR(TIMER1_OVF_vect) {
	if (type == TIMER_NONE) {
		return;
	}
	totalSec += OVF_SECONDS;
	if (totalSecCorrector++ == 10) {
		// Корректируем каждые 10 сек
		totalSecCorrector = 0;
		totalSec += CORRECTOR;
	}
	if (totalSec >= SECONDS_IN_MINUTE) {
		// Минута прошла
		totalSec = 0;
		totalMin++;
		if (type == MINUTES && totalMin == tickCnt) {
			if (l != 0) {
				l(cmd);
			}
			totalMin = 0;
			type = TIMER_NONE;
			return;
		} else if(type == HOURS && totalMin == MINUTES_IN_HOUR) {
			// Час прошел
			totalMin = 0;
			totalHour++;
			if (totalHour == tickCnt) {
				if (l != 0) {
					l(cmd);
				}
				totalHour = 0;
				type = TIMER_NONE;
			}
		}
	}
}

void TimerSetListener(TimerType t, unsigned char cnt, unsigned char t_cmd) {
	type = t;
	tickCnt = cnt;
	totalSecCorrector = 0;
	totalSec = 0;
	totalMin = 0;
	totalHour = 0;
	cmd = t_cmd;
}

void TimerStart(onTimer cb) {
	l = cb;
	ENABLE_TIMER1_OVF_INTERRUPT();
	SET_TIMER1_PRESCALER();
}

void TimerStop() {
	DISABLE_TIMER1_OVF_INTERRUPT();
	SET_TIMER1_PRESCALER_0();
}