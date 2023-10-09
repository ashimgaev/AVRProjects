/*
 * Water.c
 *
 * Created: 19.05.2016 15:06:37
 *  Author: ruinashi
 */ 

#define F_CPU 960000UL

#include <avr/interrupt.h>
#include "ioports.h"


//#define TEST_SEQUENSE

#define POWER_MODE_RESET_MASK	(0xc7)
#define SET_POWER_DOWN_MODE()	MCUCR &= POWER_MODE_RESET_MASK; MCUCR |= ((1 << SE) | (1 << SM1));// allow power down mode

#define WDT_MASK_PRESCALLER	(0x27)
#define WDT_ENABLE_INTERRUPT()		WDTCR |= (1 << WDTIE); // Включить WDT
#define WDT_DISABLE_INTERRUPT()		WDTCR &= ~(1 << WDTIE); // Выключить WDT
#define WDT_16_MS					(0) // WDT Prescaller 2K for 0.016 sec
#define WDT_125_MS					((1<<WDP1)|(1<<WDP0)) // WDT Prescaller 16K for 0.125 sec
#define WDT_500_MS					((1<<WDP2)|(1<<WDP0)) // WDT Prescaller 64K for 0.5 sec
#define WDT_2_SEC					((1<<WDP2)|(1<<WDP1)|(1<<WDP0)) // WDT Prescaller 256K for 2 sec
#define WDT_4_SEC					(1<<WDP3); // WDT Prescaller 512K for 4 sec

#define WDT_SET_TIMEOUT(t)			do { \
										uint8_t val = WDTCR; \
										val &= ~(WDT_MASK_PRESCALLER); \
										val |= t; \
										WDTCR |= (1<<WDCE); \
										WDTCR = val; \
									} while (0)
									
#define POWER_PIN			PIN2
#define SENSOR_PIN			PIN0
#define BUTTON_PIN			PIN3
#define SOUND_PIN			PIN4

// Power macros
#define POWER_ON()		IOPORT_SET_BIT(PORTB, POWER_PIN)
#define POWER_OFF()		IOPORT_RESET_BIT(PORTB, POWER_PIN)

// Sound macros
#define SOUND_ON()		IOPORT_SET_BIT(PORTB, SOUND_PIN)
#define SOUND_OFF()		IOPORT_RESET_BIT(PORTB, SOUND_PIN)

// Button macros
#define BUTTON_ON()		IOPORT_SET_BIT(PORTB, BUTTON_PIN)
#define BUTTON_OFF()	IOPORT_RESET_BIT(PORTB, BUTTON_PIN)

// Sensor macros
#define GET_SENSOR_VALUE() IOPORT_GET_BIT(PORTB, SENSOR_PIN)
#define WATTER_FULL		1
#define WATTER_EMPTY	0

#define CMD_NONE				0
#define CMD_SLEEP				1
#define CMD_DEEP_SLEEP			2
#define CMD_CHECK_SENSOR		3

void InitPorts() {
	// Рабочие порты
	IOPORT_PIN_FOR_OUTPUT(PORTB, BUTTON_PIN); // Порт кнопки
	IOPORT_PIN_FOR_OUTPUT(PORTB, SOUND_PIN); // Порт зуммера
	IOPORT_PIN_FOR_OUTPUT(PORTB, POWER_PIN); // Порт питания датчика
	IOPORT_PIN_FOR_INPUT(PORTB, SENSOR_PIN); // Порт датчика воды
	
	// Нерабочие порты - ставим внутреннию подтяжку к VCC
	IOPORT_PIN_DISABLE(PORTB, PIN1);
}


#define WDT_DELAY(t)	do { \
							WDT_SET_TIMEOUT(t); \
							WDT_ENABLE_INTERRUPT(); \
							asm("sleep"); \
							WDT_DISABLE_INTERRUPT(); \
						} while(0)


#define ALARM_TYPE_BUTTON_PRESS	3
#define ALARM_TYPE_WAIT_WATER	1

void alarm(uint8_t type) {
	uint8_t i = type;
	
	while (i > 0) {
		SOUND_ON();
		WDT_DELAY(WDT_125_MS);
		SOUND_OFF();
		WDT_DELAY(WDT_125_MS);
		i--;
	}
}

volatile uint8_t cmd;
volatile uint8_t fisrtStart;

ISR(WDT_vect) {
	POWER_OFF();
	cmd = CMD_CHECK_SENSOR;
}

int main(void)
{
	InitPorts();
	
	sei();
	
	cmd = CMD_CHECK_SENSOR;
	fisrtStart = 1;
	
	SET_POWER_DOWN_MODE();
	
#ifdef TEST_SEQUENSE
	while(1) {
		POWER_ON();
		WDT_DELAY(WDT_4_SEC);
		POWER_OFF();
		alarm(ALARM_TYPE_WAIT_WATER);
		WDT_DELAY(WDT_4_SEC);
		alarm(ALARM_TYPE_BUTTON_PRESS);
		WDT_DELAY(WDT_4_SEC);
		BUTTON_ON();
		WDT_DELAY(WDT_4_SEC);
		BUTTON_OFF();
	}
#else
    while(1)
    {
		switch(cmd) {
			case CMD_CHECK_SENSOR:
				POWER_ON();
				WDT_DELAY(WDT_16_MS);
				if (GET_SENSOR_VALUE() == WATTER_EMPTY) {
					if (fisrtStart == 1) {
						fisrtStart = 0;
						alarm(ALARM_TYPE_WAIT_WATER);
					}
					cmd = CMD_SLEEP;
				} else {
					WDT_DELAY(WDT_500_MS);
					if (GET_SENSOR_VALUE() == WATTER_FULL) {
						// Емкость точно полная
						alarm(ALARM_TYPE_BUTTON_PRESS);
						if (fisrtStart != 1) {
							BUTTON_ON();
							WDT_DELAY(WDT_125_MS);
							BUTTON_OFF();
						}
						cmd = CMD_DEEP_SLEEP;
					} else {
						cmd = CMD_SLEEP;
					}
				}
				POWER_OFF();
				break;
			case CMD_SLEEP:
				WDT_DELAY(WDT_2_SEC);
				cmd = CMD_CHECK_SENSOR;
				break;
			case CMD_DEEP_SLEEP:
				while(1) {
					asm("sleep");
				}
				break;
			default:
				cmd = CMD_SLEEP;
				break;
		}
    }
#endif
}
