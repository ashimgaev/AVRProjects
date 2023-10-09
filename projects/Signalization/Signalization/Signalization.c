/*
 * Signalization.c
 *
 * Created: 16.05.2014 15:06:37
 *  Author: ruinashi
 */ 

#define F_CPU 1000000UL

#include "ioports.h"
#include <avr/interrupt.h>
#include <avr/delay.h>

#define SET_TIMER_PRESCALER_64() TCCR0B=(1<<CS00)|(1<<CS02)
#define SET_TIMER_PRESCALER_0() TCCR0B=0
#define ENABLE_TIMER_OVF_INTERRUPT() TIMSK0=(1<<TOIE0)
#define DISABLE_TIMER_INTERRUPTS() TIMSK0=0
#define STOP_TIMER() SET_TIMER_PRESCALER_0();DISABLE_TIMER_INTERRUPTS()
#define START_TIMER() SET_TIMER_PRESCALER_64()

//#define CYCLE_ALARM


// Coil macros
#define SOUND_ON() IOPORT_RESET_BIT(PORTB, PIN0)
#define SOUND_OFF() IOPORT_SET_BIT(PORTB, PIN0)

// Hall defines
#define GET_HALL_SIGNAL() IOPORT_GET_BIT(PORTB, PIN1)

void InitPorts() {
	// Work ports
	IOPORT_PIN_FOR_OUTPUT(PORTB, PIN0); // Main transistor
	SOUND_OFF();
	
	IOPORT_PIN_FOR_INPUT(PORTB, PIN1); // Hall input
	
	// Other ports
	IOPORT_PIN_FOR_INPUT(PORTB, PIN2);
	IOPORT_PIN_FOR_INPUT(PORTB, PIN3);
	IOPORT_PIN_FOR_INPUT(PORTB, PIN4);
}

void InitInterrupts() {
	MCUCR = 0x0; // Low level on INT0
	GIMSK = (1<<INT0);
}

#define CMD_NONE 0
#define CMD_SLEEP 1
#define CMD_WARNING 2

volatile int cmd = CMD_NONE;

#ifdef CYCLE_ALARM
volatile int ovfCnt = 0;
volatile int inc = 1;
#endif

volatile int warnCnt = 0;
ISR(TIM0_OVF_vect) {
	warnCnt++;
#ifdef CYCLE_ALARM
	ovfCnt += inc;
	if (ovfCnt == 2) { // ovfCnt=6 ~ 1.5 sec
		SOUND_OFF();
		inc = -1;
	} else if (ovfCnt == 0) {
		SOUND_ON();
		inc = 1;
	}
#endif
	
	if (warnCnt == 300) {
		cmd = CMD_SLEEP;
	}
}

ISR(INT0_vect) {
	cmd = CMD_WARNING;
	GIMSK = 0;
}

ISR(PCINT0_vect) {
	cmd = CMD_WARNING;
}

int main(void)
{
	InitPorts();
	InitInterrupts();
	
	// wait 10 seconds before start
	_delay_ms(10000);
	
	MCUCR |= (1 << SE) | (1 << SM1); // allow power down mode
	
	if (GET_HALL_SIGNAL() == 1) {
		SOUND_ON();
		_delay_ms(150);
		SOUND_OFF();
	} else {
		asm("sleep");
	}

	
	sei();

	asm("sleep");
	
    while(1)
    {
		switch (cmd) {
		case CMD_WARNING: {
			// Disable external interrupts
			MCUCR = (1 << SE) | (1 << SM1);
			GIMSK = 0;
			
			// wait 10 sec before start warning
			_delay_ms(10000);
			
			ENABLE_TIMER_OVF_INTERRUPT();
			START_TIMER();
			SOUND_ON();
			cmd = CMD_NONE;
			break;
		}
		case CMD_SLEEP: {
			STOP_TIMER();
			SOUND_OFF();
			cmd = CMD_NONE;
			asm("sleep");
			break;
		}	
		}
    }
}
