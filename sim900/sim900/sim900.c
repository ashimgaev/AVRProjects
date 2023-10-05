/*
* sim900.c
*
* Created: 27.07.2015 11:21:38
*  Author: RUINASHI
*/

#include "defines.h"

#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "uart.h"
#include "ioports.h"
#include <util/delay.h>
#include "timer.h"
#include "dht11.h"

#include <avr/eeprom.h>

#ifdef _DEBUG_
	#warning "!!! DEBUG MODE ENABLED !!!"
#endif

#ifndef F_CPU
	#error "F_CPU not declared!"
#endif

#ifdef _DEBUG_
	#define _delay_ms(X)	while(0)
	#define _delay_us(X)	while(0)
#endif

/* 4800 baud */
#define UART_BAUD_RATE      4800

typedef unsigned char bool;
#define true 1
#define false 0

#define MY_PHONE "\"89527641166\""

const char RESPONSE_OK[] PROGMEM = "OK\r\n";
const char RESPONSE_ERROR[] PROGMEM = "ERROR\r\n";
const char RESPONSE_RDY[] PROGMEM = "RDY\r\n";
const char RESPONSE_CALL_READY[] PROGMEM = "Call Ready\r\n";
const char RESPONSE_CFUN[] PROGMEM = "+CFUN"; // Функциональность модуля(1 — полная)
const char RESPONSE_CPIN_READY[] PROGMEM = "+CPIN: READY\r\n"; // ПИН не нужен
const char RESPONSE_CMTI[] PROGMEM = "+CMTI\r\n"; // Пришла смс. Формат: +CMTI: «SM»,1
const char RESPONSE_CMGR[] PROGMEM = "+CMGR\r\n"; // Ответ при чтении смс
const char RESPONSE_NORMAL_POWD[] PROGMEM = "NORMAL POWER DOWN\r\n"; // Модуль отключился
const char RESPONSE_CREG[] PROGMEM = "+CREG:"; //
const char RESPONSE_CPAS[] PROGMEM = "+CPAS:"; //


#ifdef _DEBUG_
const char buff[] = "RDY\r\n+CFUN: 1\r\n+CPIN: READY\r\nCall Ready\r\nOK\r\nOK\r\nOK\r\nOK\r\nOK\r\nOK\r\n+CSQ: 27,0\r\nOK\r\n+CREG: 0,1\r\nOK\r\n+CPAS: 0\r\nOK\r\n>OK\r\nNORMAL POWER DOWN\r\n"; // Good test
//const char buff[] = "RDY\r\n+CFUN: 1\r\n+CPIN: READY\r\nCall Ready\r\nOK\r\nOK\r\nOK\r\nOK\r\nOK\r\nOK\r\n+CSQ: 27,0\r\nOK\r\n+CREG: 0,1\r\nOK\r\n+CPAS: 2\r\nOK\r\n>OK\r\nNORMAL POWER DOWN\r\n"; // Bad CPAS test
//const char buff[] = "RDY\r\n+CFUN: 1\r\n+CPIN: READY\r\nCall Ready\r\nOK\r\nOK\r\nOK\r\nOK\r\nOK\r\nOK\r\n+CSQ: 27,0\r\nOK\r\n+CREG: 0,3\r\nOK\r\n+CPAS: 0\r\nOK\r\n>OK\r\nNORMAL POWER DOWN\r\n"; // Bad CREG test
//const char buff[] = "RDY\r\n+CFUN: 1\r\n+CPIN: READY\r\nCall Ready\r\nOK\r\nOK\r\nOK\r\nOK\r\nOK\r\nOK\r\n+CSQ: 27,0\r\nOK\r\n+CREG: 0,2\r\nOK\r\n"; // Wait CREG test
//const char buff[] = "RDY\r\n+CFUN: 1\r\n+CPIN: READY\r\nCall Ready\r\nOK\r\n"; // No response test
//const char buff[] = "RDY\r\n+CFUN: 1\r\n+CPIN: READY\r\n"; // No "Call ready" test
//const char buff[] = "RDY\r\n+CFUN: 1\r\n+CPIN: READY\r\nCall Ready\r\nERROR\r\nNORMAL POWER DOWN\r\n"; // Error response test
volatile int index;
unsigned int uart_Getc(void) {
	if (index == strlen(buff)) {
		//index = 0; // Cycle test
		return 0x0100;
	}
	return (unsigned int)buff[index++];
}
#endif

// POWER MODE
#define SET_POWER_DOWN_MODE()	MCUCR |= (1 << SE) | (1 << SM1); // allow power down mode
#define SET_IDLE_MODE()	MCUCR |= (1 << SE); MCUCR &= 0x8f;// allow Idle mode
#define SIM900_POWER_ON_DELAY	1200 // ms
#define SIM900_POWER_OFF_DELAY	3000 // ms

// WORK PORTS SETUP
#define SIM900_RESET_PORT	PORTC
#define SIM900_RESET_PIN	PIN5
#define SIM900_POWER_PORT	PORTC
#define SIM900_POWER_PIN	PIN2

// Количество попыток в случае ошибки (ошибка ответа, модуль не отвечает, неправильное значение)
#define MAX_ERROR_COUNT	3

// Количество попыток поиска сети (пока не придет CREG 0,1)
#define MAX_SIGNAL_COUNT	2

// Количество попыток включить модуль если он сам отключается
#define MAX_RESET_COUNT	5


#define TIMER_CMD_SLEEP_COMPLETE	0
#define TIMER_CMD_READY_FAILED		1
#define TIMER_CMD_NO_RESPONSE		2
#define TIMER_CMD_SIGNAL_WAIT		3

typedef enum {
	SLEEP_REASON_TEMP_WAS_SEND = 0,
	SLEEP_REASON_MODULE_RESET,
	SLEEP_REASON_MODULE_RESET_MAX,
	SLEEP_REASON_ERROR,
	SLEEP_REASON_ERROR_MAX,
	SLEEP_REASON_NO_SIGNAL
} SleepReason;

#ifdef _DEBUG_
	#define SLEEP_MODE_TEMP_WAS_SEND		MINUTES
	#define REPORT_TEMP_PERIOD				1
	#define SLEEP_PERIOD_TEMP_WAS_SEND		1
	#define SLEEP_PERIOD_MODULE_RESET_M		1
	#define SLEEP_PERIOD_MODULE_RESET_H		1
	#define SLEEP_PERIOD_ERROR_M			1
	#define SLEEP_PERIOD_ERROR_H			1
	#define SLEEP_PERIOD_NO_SIGNAL_M		1
	#define TIMER_READY_WAIT_PERIOD_M		1
	#define TIMER_RESPONSE_WAIT_PERIOD_M	1
	#define TIMER_SIGNAL_WAIT_PERIOD_M		1
#else
	#define SLEEP_MODE_TEMP_WAS_SEND		HOURS
	#define REPORT_TEMP_PERIOD				4
	#define SLEEP_PERIOD_TEMP_WAS_SEND		1 // Время сна после отправки сообщения в часах
	#define SLEEP_PERIOD_MODULE_RESET_M		1 // Время сна в минутах если модуль отключился сам
	#define SLEEP_PERIOD_MODULE_RESET_H		1 // Время сна в часах если модуль отключился сам (когда все попытки исчерпаны)
	#define SLEEP_PERIOD_ERROR_M			10 // Время сна при ошибке в минутах
	#define SLEEP_PERIOD_ERROR_H			2 // Время сна при ошибке в часах (когда все попытки исчерпаны)
	#define SLEEP_PERIOD_NO_SIGNAL_M		30 // Время сна в минутах если не поймали сигнал
	#define TIMER_READY_WAIT_PERIOD_M		2 // Период ожидяния 'Call Ready' в минутах
	#define TIMER_RESPONSE_WAIT_PERIOD_M	1 // Период ожидяния ответа на запрос в минутах
	#define TIMER_SIGNAL_WAIT_PERIOD_M		1 // Период ожидания сигнала сети в минутах при CREG 0,2
#endif

#define SIM900_ENABLE_POWER() IOPORT_SET_BIT(SIM900_POWER_PORT, SIM900_POWER_PIN)
#define SIM900_DISABLE_POWER() IOPORT_RESET_BIT(SIM900_POWER_PORT, SIM900_POWER_PIN)

#define SIM900_POWER_OFF() \
IOPORT_SET_BIT(SIM900_RESET_PORT, SIM900_RESET_PIN); \
_delay_ms(SIM900_POWER_OFF_DELAY); \
IOPORT_RESET_BIT(SIM900_RESET_PORT, SIM900_RESET_PIN)

#define SIM900_POWER_ON() \
IOPORT_SET_BIT(SIM900_RESET_PORT, SIM900_RESET_PIN); \
_delay_ms(SIM900_POWER_ON_DELAY); \
IOPORT_RESET_BIT(SIM900_RESET_PORT, SIM900_RESET_PIN)

/******************* Объявляем команды *******************************/
#define CMD_NONE	0
// Дефолтные команды
#define CMD_ATE0	1 // Режим эха (0 - выкл, 1 - вкл)
#define CMD_CMEE	2 // Уровень информации об ошибке (0 - без описания)
#define CMD_ATV0	3 // Формат ответа модуля (0 - только код, 1 - с описанием ошибки)
#define CMD_GSMBUSY	4 // Запрет всех входящих звонков (1 - запретить, 0 - разрешить)
#define CMD_CMGF	5 // Формат смс (1 - текстовый, 0 - HEX)
#define CMD_CSCB	6 // Gрием широковещательных сообщений (1 - запретить)
#define CMD_CSQ		7 // Проверка уровня сигнала
#define CMD_DEFAULT_LAST	CMD_CSQ // Последняя дефолтная команда
// Проверка сети и работоспособности
#define CMD_CREG	8 // Получить тип регистрации в сети (1 - домашняя сеть):  AT+CREG?
#define CMD_CPAS	9 // Информация о текущем состоянии телефона (0 - готов к работе): AT+CPAS
// Работа с сообщениями
#define CMD_CMGD	10 // Удаление сообщений: AT+CMGD=1,4
#define CMD_CMGS	11 // Отправка СМС: AT+CMGS="89527431336"
// Выключение
#define CMD_CPOWD	12 // Выключение модуля (1 - вывести "Normal Power down"): AT+CPOWD=1

// Порядок команд должен совпадать с их номерами !!!
char* CMD_MAP[] = {
	"",
	"ATE0",
	"AT+CMEE=0",
	"ATV1",
	"AT+GSMBUSY=1",
	"AT+CMGF=1",
	"AT+CSCB=1",
	"AT+CSQ",
	"AT+CREG?",
	"AT+CPAS",
	"AT+CMGD=1,4",
	"AT+CMGS="MY_PHONE,
	"AT+CPOWD=1"
};
/**************************************************/

#define CMD_STATUS_OK				0
#define CMD_STATUS_UNKNOWN			1
#define CMD_STATUS_WAIT_RESPONSE	2
#define CMD_STATUS_BAD_VALUE		3
#define CMD_STATUS_ERROR			4
#define CMD_STATUS_NO_RESPONSE		5
#define CMD_STATUS_STOPPED			6
#define CMD_STATUS_NO_SIGNAL		7
#define CMD_STATUS_MSG_SEND			8

#define SET_SLEEP_TRUE()	sleep=true
#define SET_SLEEP_FALSE()	sleep=false

#define SET_SLEEP_TIME(mode,time,reason) sleepTime=time;sleepMode=mode;sleepReason=reason

volatile bool sleep;
volatile unsigned char temp;
volatile unsigned long tempSendCnt;
volatile bool bTempError;
volatile unsigned char errorCnt;
volatile unsigned char signalCnt;
volatile unsigned char rstCnt;
volatile unsigned char sleepTime;
volatile SleepReason sleepReason;
volatile TimerType sleepMode;


#define CMD_QUEUE_SIZE	16 //Размер очереди команд (должна быть степень двойки)
#define CMD_BUFFER_MASK ( CMD_QUEUE_SIZE - 1)

typedef struct cmdqueue {
	unsigned char buff[CMD_QUEUE_SIZE];
	unsigned char tail;
	unsigned char head;
} CmdQueue;

CmdQueue cmdQ;
volatile unsigned char lastCmd;
volatile unsigned char cmdStatus;

#define MSG_BUFF_SIZE	64 //Размер очереди для буфера (должна быть степень двойки)
typedef struct MSG {
	unsigned char pBuff[MSG_BUFF_SIZE];
	unsigned char cnt;
	bool isComple;
} Msg;
Msg msg;


#define FLAG_READY_TO_CALL 0x01
#define FLAG_CREG_READY 0x02
#define FLAG_CREG_WAIT 0x04
#define FLAG_CPAS_READY 0x08
#define FLAG_READY_TO_SMS (FLAG_READY_TO_CALL | FLAG_CREG_READY | FLAG_CPAS_READY)
volatile unsigned char readyFlag;

static bool AddCmd(unsigned char cmd) {
	unsigned char tmphead;
	tmphead = (cmdQ.head + 1) & CMD_BUFFER_MASK;
	if ( tmphead == cmdQ.tail ) {
		return false;
	} else {
		cmdQ.head = tmphead;
		cmdQ.buff[tmphead] = cmd;
	}
	return true;
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

static void TimerCallback(unsigned char cmd) {
	switch (cmd) {
		case TIMER_CMD_SLEEP_COMPLETE:
			SET_SLEEP_FALSE();
			break;
		case TIMER_CMD_NO_RESPONSE:
			cmdStatus = CMD_STATUS_NO_RESPONSE;
			break;
		case TIMER_CMD_READY_FAILED:
			cmdStatus = CMD_STATUS_ERROR;
			break;
		case TIMER_CMD_SIGNAL_WAIT:
			AddCmd(CMD_CREG);
			break;
	}
}


static void SendCmd(unsigned char cmd) {
	uart_puts(CMD_MAP[cmd]);
	uart_puts("\r\n");
	lastCmd = cmd;
	cmdStatus = CMD_STATUS_WAIT_RESPONSE;
	TimerSetListener(MINUTES, TIMER_RESPONSE_WAIT_PERIOD_M, TIMER_CMD_NO_RESPONSE);
}


static void ProcessNextCmd() {
	unsigned char cmd = GetCmd();
	if (cmd != CMD_NONE) {
		SendCmd(cmd);
	}
}

static void StopSafe() {
#ifdef _DEBUG_
	index = 0;
#endif
	_delay_ms(100);
	SIM900_DISABLE_POWER();
	
	TimerSetListener(TIMER_NONE, 0, 0);
	
	if (sleep == true) {
		TimerSetListener(sleepMode, sleepTime, TIMER_CMD_SLEEP_COMPLETE);
		// Смотрим почему мы уходим в сон
		switch (sleepReason) {
			case SLEEP_REASON_MODULE_RESET_MAX:
			case SLEEP_REASON_ERROR_MAX:
			case SLEEP_REASON_NO_SIGNAL:
			case SLEEP_REASON_TEMP_WAS_SEND:
				errorCnt = MAX_ERROR_COUNT-1;
				rstCnt = MAX_RESET_COUNT-1;
				if (sleepReason == SLEEP_REASON_TEMP_WAS_SEND) {
					tempSendCnt++;
				}
				break;
			case SLEEP_REASON_MODULE_RESET:
				errorCnt = MAX_ERROR_COUNT-1;
				rstCnt--;
				break;
			case SLEEP_REASON_ERROR:
				rstCnt = MAX_RESET_COUNT-1;
				errorCnt--;
				break;
		}
		
		while (sleep == true) {
			asm("sleep");
		}
	}
	
	SET_SLEEP_TIME(SLEEP_MODE_TEMP_WAS_SEND, SLEEP_PERIOD_TEMP_WAS_SEND, SLEEP_REASON_TEMP_WAS_SEND);
	
	TimerSetListener(TIMER_NONE, 0, 0);
	signed char t = -1;
	signed int tmp = 0;
	char tryCnt = 5;
	char passed = 0;
	bTempError = false;
	
	unsigned char addr = 0;
	
	_delay_ms(1000); // Задержка для датчика перед работой (см. доку)
	while (tryCnt) {
		t = dht11_gettemperature();
		eeprom_write_byte(addr, t);
		addr++;
		if (t >= 0) {
			tmp += t;
			passed++;
		}
		tryCnt--;
		_delay_ms(1000); // Интервал между измерениями (см. доку)
	}
	
	if (passed == 0) {
		// Ошибка датчика температуры
		bTempError = true;
	} else {
		tmp = tmp / passed;
		temp = (unsigned char)tmp;
	}
	
	signalCnt = MAX_SIGNAL_COUNT;
	lastCmd = CMD_NONE;
	cmdStatus = CMD_STATUS_UNKNOWN;
	readyFlag = 0;
	memset(&cmdQ, 0, sizeof(CmdQueue));
	memset(&msg, 0, sizeof(msg));
	uart_reset_buffers();
	
	TimerSetListener(MINUTES, TIMER_READY_WAIT_PERIOD_M, TIMER_CMD_READY_FAILED);
	
	if ((tempSendCnt % REPORT_TEMP_PERIOD) == 0 || temp <= 8) {
		// Каждые 6 часов или если temp <= 8
		// Включаем модуль
		SIM900_ENABLE_POWER();
		_delay_ms(100);
		SIM900_POWER_ON();
		return;
	}
	else {
		cmdStatus = CMD_STATUS_STOPPED;
	}
}

#define IS_FLAG_EMPTY(FLAG)	((readyFlag & FLAG) == 0)
#define IS_FLAG_SET(FLAG)	((readyFlag & FLAG) == FLAG)

int main(void)
{
	SET_IDLE_MODE();
	
	uart_init( UART_BAUD_SELECT(UART_BAUD_RATE,F_CPU) );
	sei();
	
	tempSendCnt = 0;
	errorCnt = MAX_ERROR_COUNT-1;
	rstCnt = MAX_RESET_COUNT-1;
	signalCnt = MAX_SIGNAL_COUNT;
	SET_SLEEP_FALSE();
		
	IOPORT_PIN_FOR_OUTPUT(PORTC, SIM900_RESET_PIN);
	IOPORT_RESET_BIT(SIM900_RESET_PORT, SIM900_RESET_PIN);
	IOPORT_PIN_FOR_OUTPUT(PORTC, SIM900_POWER_PIN);
	IOPORT_RESET_BIT(SIM900_POWER_PORT, SIM900_POWER_PIN);
		
	MCUCR |= (1 << SE); // Idle mode

	StopSafe();
	TimerStart(TimerCallback);

	unsigned short int c; // UART char
	char buffer[10] = {0}; // Буфер для температуры
	volatile bool bResetTimer = false;

	for(;;)
	{
		/*
		* Get received character from ringbuffer
		* uart_getc() returns in the lower byte the received character and
		* in the higher byte (bitmask) the last receive error
		* UART_NO_DATA is returned when no data is available.
		*
		*/
#ifdef _DEBUG_
		c = uart_Getc();
#else
		c = uart_getc();
#endif
		if ( c & UART_NO_DATA ) {
			// No data
		} else {
			if (c & UART_ERROR_MASK) {
				// Error in UART
			} else {
				msg.pBuff[msg.cnt] = (unsigned char)c;
				
				if (msg.pBuff[msg.cnt] == '\n') {
					msg.isComple = true;
				} else if (lastCmd == CMD_CMGS && cmdStatus == CMD_STATUS_WAIT_RESPONSE  && msg.pBuff[msg.cnt] == '>') {
					msg.isComple = true;
					msg.cnt = 3; // To catch '>' symbol in handler below
				}
				msg.cnt++;
				
				if (msg.cnt == MSG_BUFF_SIZE) {
					msg.cnt = 0;
					msg.isComple = false;
				} else if (msg.isComple == true && msg.cnt <= 2) {
					msg.cnt = 0;
					msg.isComple = false;
				} else if (msg.isComple == true) {
					bResetTimer = true;
					if (memcmp_PF(msg.pBuff, RESPONSE_OK, 4) == 0) {
						if (cmdStatus == CMD_STATUS_BAD_VALUE) {
							cmdStatus = CMD_STATUS_ERROR;
						} else {
							cmdStatus = CMD_STATUS_OK;
						}
	
					} else if (memcmp_PF(msg.pBuff, RESPONSE_ERROR, 7) == 0) {
						cmdStatus = CMD_STATUS_ERROR;
					} else if (IS_FLAG_EMPTY(FLAG_READY_TO_CALL) && memcmp_PF(msg.pBuff, RESPONSE_CALL_READY, 12) == 0) {
						readyFlag |= FLAG_READY_TO_CALL;
						volatile unsigned char i = 1;
						for (i; i<=CMD_DEFAULT_LAST; i++) {
							AddCmd(i); // Добавляем дефолтные команды для настройки модуля
						}
						AddCmd(CMD_CREG);
						cmdStatus = CMD_STATUS_OK;
					} else if (IS_FLAG_EMPTY(FLAG_CREG_READY) && memcmp_PF(msg.pBuff, RESPONSE_CREG, 6) == 0) { 
						if (msg.pBuff[9] == '1') { // Сетка готова
							readyFlag |= FLAG_CREG_READY;
							readyFlag &=~(FLAG_CREG_WAIT);
							AddCmd(CMD_CPAS);
						} else if (msg.pBuff[9] == '2') { // Идет поиск сети
							cmdStatus = CMD_STATUS_NO_SIGNAL;
						} else {
							cmdStatus = CMD_STATUS_BAD_VALUE;
						}
					} else if (IS_FLAG_EMPTY(FLAG_CPAS_READY) && memcmp_PF(msg.pBuff, RESPONSE_CPAS, 6) == 0) {
						if (msg.pBuff[7] == '0') { // Модуль готов к работе
							readyFlag |= FLAG_CPAS_READY;
				#ifdef SEND_MSG
							AddCmd(CMD_CMGS); // Отправляем сообщение
				#else
							cmdStatus = CMD_STATUS_MSG_SEND;
				#endif
						} else {
							cmdStatus = CMD_STATUS_BAD_VALUE;
						}
					} else if ((readyFlag == FLAG_READY_TO_SMS) && msg.pBuff[0] == '>') {
						itoa(temp, buffer, 10);
						uart_puts("Temp: ");
						if (bTempError == true) {
							uart_puts("ERROR!");
						} else {
							uart_puts(buffer);
						}
						uart_putc('\x1A'); // Send Ctrl+Z
						cmdStatus = CMD_STATUS_MSG_SEND;
					} else if (memcmp_PF(msg.pBuff, RESPONSE_NORMAL_POWD, 19) == 0) {
						// Модуль отключен. Завершаем корректно работу
						cmdStatus = CMD_STATUS_STOPPED;
					} else {
						bResetTimer = false;
					}
					msg.cnt = 0;
					msg.isComple = false;
					if (bResetTimer == true) {
						if (IS_FLAG_EMPTY(FLAG_CREG_WAIT)) {
							TimerSetListener(TIMER_NONE, 0, 0);
						}
						bResetTimer = false;
					}
				}
			}
		}
		
		switch (cmdStatus) {
			case CMD_STATUS_UNKNOWN:
			case CMD_STATUS_BAD_VALUE:
			case CMD_STATUS_WAIT_RESPONSE:
				break;
			case CMD_STATUS_OK:
				ProcessNextCmd();
				break;
			case CMD_STATUS_MSG_SEND:
				SET_SLEEP_TIME(SLEEP_MODE_TEMP_WAS_SEND, SLEEP_PERIOD_TEMP_WAS_SEND, SLEEP_REASON_TEMP_WAS_SEND);
				AddCmd(CMD_CMGD); // Очищаем все смс-ки
				AddCmd(CMD_CPOWD); // Шлём команду POWER_DOWN
				cmdStatus = CMD_STATUS_WAIT_RESPONSE;
				break;
			case CMD_STATUS_NO_SIGNAL:
				signalCnt--;
				if (signalCnt == 0) {
					SET_SLEEP_TIME(MINUTES, SLEEP_PERIOD_NO_SIGNAL_M, SLEEP_REASON_NO_SIGNAL);
					AddCmd(CMD_CPOWD); // Шлём команду POWER_DOWN
				} else {
					readyFlag |= FLAG_CREG_WAIT;
					TimerSetListener(MINUTES, TIMER_SIGNAL_WAIT_PERIOD_M, TIMER_CMD_SIGNAL_WAIT);
				}
				cmdStatus = CMD_STATUS_WAIT_RESPONSE;
				break;
			case CMD_STATUS_NO_RESPONSE:
				if (rstCnt == 0) {
					// Все попытки закончились
					SET_SLEEP_TIME(HOURS, SLEEP_PERIOD_MODULE_RESET_H, SLEEP_REASON_MODULE_RESET_MAX);
				} else {
					SET_SLEEP_TIME(MINUTES, SLEEP_PERIOD_MODULE_RESET_M, SLEEP_REASON_MODULE_RESET);
				}
				SET_SLEEP_TRUE();
				StopSafe();
				break;
			case CMD_STATUS_ERROR:
				if (errorCnt == 0) {
					// Все попытки закончились
					SET_SLEEP_TIME(HOURS, SLEEP_PERIOD_ERROR_H, SLEEP_REASON_ERROR_MAX);
				} else {
					SET_SLEEP_TIME(MINUTES, SLEEP_PERIOD_ERROR_M, SLEEP_REASON_ERROR);
				}
				AddCmd(CMD_CPOWD);
				cmdStatus = CMD_STATUS_OK;
				break;
			case CMD_STATUS_STOPPED:
				SET_SLEEP_TRUE();
				StopSafe();
				break;
		}

	}
	
}
