/*
 * Signalization.c
 *
 * Created: 16.05.2014 15:06:37
 *  Author: ruinashi
 */ 

// No prescaler - CKDIV8=1 fuse
#define F_CPU 4800000UL // CKSEL1=0 CKSEL1=1 fuses for internal 4.8MHz

#include "ioports.h"
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/io.h>

#ifdef DEBUG
	#define SIMULATOR
#endif

// #define TEST_PINS

#ifdef SIMULATOR
	#define _delay_ms(X)	while(0)
	#define _delay_us(X)	while(0)
#endif

#define SET_TIMER_NO_CLOCK_SOURCE()   TCCR0B &= ~(1 << CS02) | ~(1 << CS01) | ~(1 << CS00)                             // 000
#define SET_TIMER_NO_PRESCALAR()      TCCR0B |= (TCCR0B & ~(1 << CS02)) | (TCCR0B & ~(1 << CS01)) | (1 << CS00)        // 001
#define SET_TIMER_1024_PRESCALAR()    TCCR0B |= (1 << CS02) | (TCCR0B & ~(1 << CS01))| (1 << CS00)                     // 101
#define ENABLE_TIMER_OVF_INTERRUPT() TIMSK0=(1<<TOIE0)
#define DISABLE_TIMER_INTERRUPTS() TIMSK0=0

/*
1 overflow in second = (TIMER_PRESCALAR*TIMER_REGISTER_SIZE)/F_CPU = (TIMER_PRESCALAR*256)/F_CPU sec
*/

#define STOP_TIMER() SET_TIMER_NO_CLOCK_SOURCE();DISABLE_TIMER_INTERRUPTS()
#define START_TIMER() SET_TIMER_1024_PRESCALAR()

// INT0 interrupts
#define SET_INT0_LOW_LEVEL_INTERRUPT() MCUCR &=~ ((1<<ISC01) | (1<<ISC00)) // The low level of INT0 generates an interrupt request.
#define ENABLE_SENSOR_INTERRUPT() SET_INT0_LOW_LEVEL_INTERRUPT(); GIMSK |= (1<<INT0); // Enable external interrupts for INT0
#define DISABLE_SENSOR_INTERRUPT() GIMSK &=~ (1<<INT0); // Disable external interrupts for INT0

//#define CYCLE_ALARM


// Pins in use
#define SOUND_PIN PINB0
#define CAMERA_PIN PINB2
#define LED_PIN PINB4
#define DOOR_SENSOR_PIN PINB1


// Coil macros
#define SOUND_ON() IOPORT_SET_BIT(PORTB, SOUND_PIN)
#define SOUND_OFF() IOPORT_RESET_BIT(PORTB, SOUND_PIN)

// Video macros
#define CAMERA_ON() IOPORT_SET_BIT(PORTB, CAMERA_PIN)
#define CAMERA_OFF() IOPORT_RESET_BIT(PORTB, CAMERA_PIN)

// Led macros
#define LED_ON() IOPORT_SET_BIT(PORTB, LED_PIN)
#define LED_OFF() IOPORT_RESET_BIT(PORTB, LED_PIN)

// Hall defines
#define DOOR_CLOSED 1
#define DOOR_OPENED 0
#define DOOR_STATUS() IOPORT_GET_BIT(PORTB, DOOR_SENSOR_PIN)

typedef uint8_t DoorStatysType;

void InitPorts() {
	// Output
	IOPORT_PIN_FOR_OUTPUT(PORTB, SOUND_PIN); // Pin to sound control transistor
	IOPORT_PIN_FOR_OUTPUT(PORTB, CAMERA_PIN); // Pin to camera control transistor
	IOPORT_PIN_FOR_OUTPUT(PORTB, LED_PIN); // Pin to led
	// Input
	IOPORT_PIN_FOR_INPUT(PORTB, DOOR_SENSOR_PIN); // Door sensor input
	// Other unused ports
	IOPORT_PIN_FOR_INPUT(PORTB, PINB3);
	IOPORT_SET_BIT(PORTB, PINB3);
	
	SOUND_OFF();
	CAMERA_OFF();
	LED_OFF();
}

#define CMD_NONE 0
#define CMD_SLEEP 1
#define CMD_WARNING 2

volatile int cmd = CMD_NONE;

ISR(INT0_vect) {
	cmd = CMD_WARNING;
	DISABLE_SENSOR_INTERRUPT();
}

#define PROG_STATE_ACTIVE 1
#define PROG_STATE_DONE 0

void doBeep(uint8_t cnt) {
	do {
		SOUND_ON();
		 _delay_ms(150);
		SOUND_OFF();
		_delay_ms(150);
		
	} while(--cnt != 0);
}

void doLedBlink(uint8_t cnt) {
	do {
		LED_ON();
		_delay_ms(150);
		LED_OFF();
		_delay_ms(150);
		
	} while(--cnt != 0);
}

void chipSleep(uint8_t progStateFlag) {
	sleep_enable();
	if(progStateFlag == PROG_STATE_ACTIVE) {
		sei(); // We are in working mode. Allow interrupts
	}
	sleep_cpu();
}

void do_delay_sec(uint8_t secs) {
	do {
		_delay_ms(1000);
	} while (--secs != 0);
}

DoorStatysType getDoorStatus() {
	if (DOOR_STATUS() == DOOR_CLOSED) {
		_delay_ms(200); // threshold
		if(DOOR_STATUS() == DOOR_CLOSED) {
			return DOOR_CLOSED;
		}
	}
	return DOOR_OPENED;
}

int main(void)
{
	InitPorts();
	
	// wait 2 seconds before start
	LED_ON();
	do_delay_sec(2);
	LED_OFF();
	
	
#ifdef TEST_PINS
	{
		volatile int x = 3;
		while(x-- > 0) {
			CAMERA_ON();
			SOUND_ON();
			do_delay_sec(5);
			SOUND_OFF();	
			CAMERA_OFF();
			do_delay_sec(5);		
		}
	}
#endif
	
		// Repeat till door will be closed
	while(getDoorStatus() != DOOR_CLOSED) {
		// The door is still open
		doLedBlink(3);
		do_delay_sec(5);
	}
	
	// The door is closed
	doBeep(1);
	
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	cli();
		
	ENABLE_SENSOR_INTERRUPT();
	chipSleep(PROG_STATE_ACTIVE);

    while(1)
    {
		switch (cmd) {
		case CMD_WARNING: {
			// Disable sensor interrupts
			DISABLE_SENSOR_INTERRUPT();
			// Turn on camera
			CAMERA_ON();
			// wait 10 sec before start warning
			do_delay_sec(10);
			
			// Start alarm
			SOUND_ON();
			do_delay_sec(60);
			SOUND_OFF();
			
			do_delay_sec(60);
			CAMERA_OFF();
			
			cmd = CMD_SLEEP;
			break;
		}
		case CMD_SLEEP: {
			DISABLE_SENSOR_INTERRUPT();
			cmd = CMD_NONE;
			chipSleep(PROG_STATE_DONE);
			break;
		}	
		}
    }
}
