/*
 * Glade.c
 *
 * Created: 16.09.2015 15:06:37
 *  Author: ruinashi
 */ 

#define F_CPU 960000UL

#include "ioports.h"
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <string.h>
#include <avr/wdt.h>

//#define SIMULATOR
#define USE_WATCH_DOG

#ifdef SIMULATOR
#define _delay_ms(X)	while(0)
#define _delay_us(X)	while(0)
#endif

#define POWER_MODE_RESET_MASK	0xc7
#define SET_IDLE_MODE()			MCUCR &= POWER_MODE_RESET_MASK; MCUCR |= (1 << SE); // allow idle mode
#define SET_POWER_DOWN_MODE()	MCUCR &= POWER_MODE_RESET_MASK; MCUCR |= ((1 << SE) | (1 << SM1));// allow power down mode

#define ENABLE_BUTTON_INTERRUPT() GIMSK |= (1<<PCIE); // Enable external interrupts for PCINTx
#define DISABLE_BUTTON_INTERRUPT() GIMSK &=~ (1<<PCIE); // Disable external interrupts for PCINTx

#define ENABLE_SENSOR_INTERRUPT() GIMSK |= (1<<INT0); // Enable external interrupts for INT0
#define DISABLE_SENSOR_INTERRUPT() GIMSK &=~ (1<<INT0); // Disable external interrupts for INT0

#define SET_LOGIC_CHANGE_INTERRUPT() MCUCR |= (1<<ISC00) // Any logical change on INT0 generates an interrupt request.
#define SET_LOW_LEVEL_INTERRUPT() MCUCR &=~ ((1<<ISC01) | (1<<ISC00)) // Any logical change on INT0 generates an interrupt request.

#define SET_TIMER_PRESCALER_1() TCCR0B=(1<<CS00) // 1ovf = 0.00025sec (1Mhz)
#define SET_TIMER_PRESCALER_1024() TCCR0B=(1<<CS02)|(1<<CS00) // 1ovf = 0.33sec (1Mhz)
#define SET_TIMER_PRESCALER_0() TCCR0B=0
#define ENABLE_TIMER_OVF_INTERRUPT() TIMSK0=(1<<TOIE0)
#define DISABLE_TIMER_INTERRUPTS() TIMSK0=0

#define START_LIGHT_TIMER() TCNT0=0;SET_TIMER_PRESCALER_1024(); // Отслеживает сколько был включен свет
#define START_MOTOR_TIMER() TCNT0=0;SET_TIMER_PRESCALER_1(); // Следит за моторчиком
#define STOP_TIMER() TCNT0=0;SET_TIMER_PRESCALER_0();

#define MOTOR_PIN			PIN4
#define SENSOR_PIN			PIN1
#define BUTTON_PIN			PIN3


// Motor macros
#define MOTOR_ON() IOPORT_SET_BIT(PORTB, MOTOR_PIN)
#define MOTOR_OFF() IOPORT_RESET_BIT(PORTB, MOTOR_PIN)

//#define MOTOR_TIME_CNT	0x0807 // Prescaller 1, 1Mhz

// Sensor macros
#define GET_SENSOR_VALUE() IOPORT_GET_BIT(PORTB, SENSOR_PIN)
#define LIGHT_ON	0
#define LIGHT_OFF	1
#ifdef SIMULATOR
#define LIGHT_ON_MAX_TIME	2
#else
#define LIGHT_ON_MAX_TIME	400 // ~1,5min (Prescaller - 1024, 1Mhz)
#endif

// Button macros
#define GET_BUTTON_VALUE() IOPORT_GET_BIT(PORTB, BUTTON_PIN)
#define BUTTON_PRESSED	0

#define CMD_NONE 0
#define CMD_START_MOTOR 1
#define CMD_SLEEP 2
#define CMD_CHECK_SENSOR 3
#define CMD_IDLE 4

volatile uint8_t cmd = CMD_NONE;
volatile uint16_t ovfCnt;
volatile bool bWakeUp;


#define CMD_QUEUE_SIZE	8 //Размер очереди команд (должна быть степень двойки)
#define CMD_BUFFER_MASK ( CMD_QUEUE_SIZE - 1)

typedef struct cmdqueue {
	unsigned char buff[CMD_QUEUE_SIZE];
	unsigned char tail;
	unsigned char head;
} CmdQueue;

CmdQueue cmdQ;

static void AddCmd(unsigned char cmd) {
	unsigned char tmphead;
	tmphead = (cmdQ.head + 1) & CMD_BUFFER_MASK;
	if (tmphead != cmdQ.tail) {
		cmdQ.head = tmphead;
		cmdQ.buff[tmphead] = cmd;
	}
	return;
}

static unsigned char GetCmd() {
	unsigned char tmptail;
	unsigned char data;

	if ( cmdQ.head == cmdQ.tail ) {
		return CMD_NONE;
	}
	
	tmptail = (cmdQ.tail + 1) & CMD_BUFFER_MASK;
	cmdQ.tail = tmptail;
	data = cmdQ.buff[tmptail];
	return data;
}

void InitPorts() {
	// Рабочие порты
	IOPORT_PIN_FOR_INPUT(PORTB, BUTTON_PIN); // Порт кнопки
	IOPORT_SET_BIT(PORTB, BUTTON_PIN); // Включаем подтягивающий резистор к VCC
	IOPORT_PIN_FOR_INPUT(PORTB, SENSOR_PIN); // Порт датчика света
	IOPORT_PIN_FOR_OUTPUT(PORTB, MOTOR_PIN); // Порт моторчика
	
	// Нерабочие порты - ставим подтяжку к VCC
	IOPORT_PIN_FOR_INPUT(PORTB, PIN0);
	IOPORT_SET_BIT(PORTB, PIN0);
	IOPORT_PIN_FOR_INPUT(PORTB, PIN2);
	IOPORT_SET_BIT(PORTB, PIN2);
}

void InitInterrupts() {
	// External interrupts
	PCMSK = (1<<PCINT3);

	// Timer interrupts
	ENABLE_TIMER_OVF_INTERRUPT();
}

ISR(TIM0_OVF_vect) {
	ovfCnt++;
	if (ovfCnt > LIGHT_ON_MAX_TIME)  {
		STOP_TIMER();
		AddCmd(CMD_START_MOTOR);
	}
}


// Light sensor interrupt
ISR(INT0_vect) {
	bWakeUp = true;
	DISABLE_SENSOR_INTERRUPT();
	AddCmd(CMD_CHECK_SENSOR);
}


#ifdef USE_WATCH_DOG
// WDT interrupt
#define SLEEP_MAX_TIME	6 // в минутах
#ifdef SIMULATOR
#define SLEEP_MAX_TICKS	2
#else
#define SLEEP_MAX_TICKS	((SLEEP_MAX_TIME * 60) / 8)
#endif
ISR(WDT_vect) {
	ovfCnt += 1;
}
#endif

//Button interrupt
ISR(PCINT0_vect) {
	_delay_ms(100);
	if (GET_BUTTON_VALUE() == BUTTON_PRESSED) {
		AddCmd(CMD_START_MOTOR);
		AddCmd(CMD_CHECK_SENSOR);
	}
}

int main(void)
{
	_delay_ms(5000);
	
	bWakeUp = false;
	volatile bool bStartMotorOnLight = false;
	InitPorts();
	InitInterrupts();
	

#ifdef USE_WATCH_DOG
	// Set WDT Prescaller for 8sec
	WDTCR |= (1<<WDCE);
	WDTCR |= (1<<WDP3)|(1<<WDP0); // WDT Prescaller 1024
#endif

	sei();

	AddCmd(CMD_START_MOTOR);
	
    while(1)
    {
		cmd = GetCmd();
		switch(cmd) {
			case CMD_CHECK_SENSOR:
				if (GET_SENSOR_VALUE() == LIGHT_OFF) {
					SET_LOW_LEVEL_INTERRUPT();
					DISABLE_BUTTON_INTERRUPT();
					STOP_TIMER();
					if (ovfCnt > LIGHT_ON_MAX_TIME) {
						bStartMotorOnLight = true;
						AddCmd(CMD_START_MOTOR);
						AddCmd(CMD_START_MOTOR); // Первый раз не прыскает почему-то (залипает жидкость?)
						AddCmd(CMD_CHECK_SENSOR);
					} else {
						AddCmd(CMD_SLEEP);
					}
				} else {
					SET_LOGIC_CHANGE_INTERRUPT();
					ENABLE_BUTTON_INTERRUPT();
					START_LIGHT_TIMER();
					if (bStartMotorOnLight == true) {
						bStartMotorOnLight = false;
						AddCmd(CMD_START_MOTOR);
						AddCmd(CMD_CHECK_SENSOR);
					}
				}
				ovfCnt = 0;
				ENABLE_SENSOR_INTERRUPT();
				break;
			case CMD_START_MOTOR:
			    //STOP_TIMER();
				// отключаем кнопку на время
				DISABLE_BUTTON_INTERRUPT();
				DISABLE_SENSOR_INTERRUPT(); // Будет просадка напряжения - можем поймать light off
				MOTOR_ON();
				_delay_ms(513);
				MOTOR_OFF();
				//ovfCnt = 0;
				_delay_ms(2000); // Даём вернуться рычагу назад
				break;
			case CMD_SLEEP:
				SET_POWER_DOWN_MODE();
				SET_LOW_LEVEL_INTERRUPT();
				ovfCnt = 0;
				bWakeUp = false;
#ifdef USE_WATCH_DOG
				WDTCR |= (1 << WDTIE); // Включить WDT
				while (bWakeUp == false) {
					asm("sleep");
					if (ovfCnt > SLEEP_MAX_TICKS) {
						WDTCR&=~(1 << WDTIE); // Выключить WDT
						bStartMotorOnLight = false;
					}
					if (GET_SENSOR_VALUE() == LIGHT_ON) {
						bWakeUp = true;
					}
				}
#else
				asm("sleep");
#endif

				bWakeUp = false;
				ovfCnt = 0;
#ifdef USE_WATCH_DOG
				WDTCR&=~(1 << WDTIE); // Выключить WDT
#endif
				memset(&cmdQ, 0, sizeof(cmdQ));
				AddCmd(CMD_CHECK_SENSOR);
				break;
			default:
				SET_IDLE_MODE();
				asm("sleep");
				break;
		}
    }
}
