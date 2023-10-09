#define F_CPU 10000000UL

#if F_CPU == 10000000UL // 10 Mgz
	#define COMPA_INCREMENT 4882 // 4882 For 10 MGz and prescaller 1024 - 500ms
	#define COMPB_INCREMENT 1000
#else if F_CPU == 1000000UL // 1Mgz
	#define COMPA_INCREMENT 488 // 4882 For 10 MGz and prescaller 1024 - 500ms
	#define COMPB_INCREMENT 100
#endif

#include "ioports.h"
#include <avr/interrupt.h>
#include <avr/delay.h>

#define SPEED_LIMIT 75

#define TRUE 1
#define FALSE 0

unsigned char digits[] = {0x5f, 0x6, 0x3b, 0x2f, 0x66, 0x6d, 0x7d, 0x7, 0x7f, 0x6f};

volatile unsigned int currSpeed = 0;
volatile unsigned int sigCnt = 0;
volatile unsigned int sigCntTmp = 0;
volatile unsigned char speedCtrlEnabled = FALSE;

#define D3 0
#define D2 1
#define D1 2

unsigned char diplayData[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
volatile unsigned char currD = D2;
volatile unsigned char bWarning = FALSE;

#define SOUND_ON 1
#define SOUND_OFF 0
unsigned char beepMap[] = {SOUND_ON, SOUND_OFF, SOUND_ON, SOUND_OFF, SOUND_ON, SOUND_OFF};
volatile unsigned char beepIndex = 0;
volatile unsigned char bLedOn = FALSE;

#define SET_TIMER1_PRESCALER() TCCR1B=(1<<CS12)|(1<<CS10) // 1024
//#define SET_TIMER1_PRESCALER() TCCR1B=(1<<CS11) // 8
#define ENABLE_TIMER1_COMPA_INTERRUPT() TIMSK|=(1<<OCIE1A)
#define ENABLE_TIMER1_COMPB_INTERRUPT() TIMSK|=(1<<OCIE1B)
#define DISABLE_TIMER1_COMPB_INTERRUPT() TIMSK&=~(1<<OCIE1B)
#define START_TIMER1() SET_TIMER1_PRESCALER()


#if F_CPU == 10000000UL // 10 Mgz
	#define SET_TIMER0_PRESCALER() TCCR0=(1<<CS02)// 256
#else if F_CPU == 1000000UL // 1Mgz
	//#define SET_TIMER0_PRESCALER() TCCR0=(1<<CS01)|(1<<CS00)// 64
	#define SET_TIMER0_PRESCALER() TCCR0=(1<<CS01) // 8
#endif
#define ENABLE_TIMER0_OVF_INTERRUPT() TIMSK|=(1<<TOIE0)
#define START_TIMER0() SET_TIMER0_PRESCALER()

// Button input
#define GET_BUTTON_INPUT_SIGNAL() IOPORT_GET_BIT(PORTD, PIN3)

// LED macros
#define LED_ON() IOPORT_SET_BIT(PORTB, PIN0)
#define LED_OFF() IOPORT_RESET_BIT(PORTB, PIN0)

// Audio macros
#define AUDIO_ON() IOPORT_SET_BIT(PORTD, PIN0)
#define AUDIO_OFF() IOPORT_RESET_BIT(PORTD, PIN0)

#define CMD_NONE 0
#define CMD_CALC_SPEED 1
#define CMD_RESET 3

volatile char cmd = CMD_NONE;

// Speed input handler
ISR(INT0_vect) {
	sigCnt++;
}

// Button input handler
ISR(INT1_vect) {
	_delay_ms(10);
	if (GET_BUTTON_INPUT_SIGNAL() == 1) {
		speedCtrlEnabled = TRUE;
	}  else {
		DISABLE_TIMER1_COMPB_INTERRUPT();
		bWarning = FALSE;
		speedCtrlEnabled = FALSE;
		beepIndex = 0;
		AUDIO_OFF();
		bLedOn = FALSE;
		LED_OFF();
	}
}

// Timer1 comparatorA handler
ISR(TIMER1_COMPA_vect) {
	cmd= CMD_CALC_SPEED;
	sigCntTmp = sigCnt;
	sigCnt = 0;
	if (speedCtrlEnabled == TRUE) {
		if (currSpeed > SPEED_LIMIT && bWarning == FALSE) {
			OCR1B = TCNT1 + 50;
			bWarning = TRUE;
			ENABLE_TIMER1_COMPB_INTERRUPT();
		} else if (currSpeed <= SPEED_LIMIT && bWarning == TRUE) {
			beepIndex = 0;
			AUDIO_OFF();
			bLedOn = FALSE;
			LED_OFF();
			DISABLE_TIMER1_COMPB_INTERRUPT();
			bWarning = FALSE;
		}
	}
	OCR1A = TCNT1 + COMPA_INCREMENT;
}

ISR(TIMER1_COMPB_vect) {
	if (bLedOn == TRUE) {
		LED_OFF();
		bLedOn = FALSE;
	} else {
		LED_ON();
		bLedOn = TRUE;
	}
	
	if (beepIndex <= 5) {
		if (beepMap[beepIndex] == SOUND_ON) {
			AUDIO_ON();
		} else {
			AUDIO_OFF();
		}
	} else if (beepIndex >= 20) { // delay between beeps cycles
		beepIndex = 0xff;
	} 
	beepIndex++;
	
	OCR1B = TCNT1 + COMPB_INCREMENT;
}

void setDigit() {
	unsigned char abcdfg = 0;
	unsigned char e = 0;
	switch (currD) {
		case D1: {
			abcdfg = 0;
			e = 1;
			break;
		}
		case D2: {
			abcdfg = 2;
			e = 3;
			break;
		}
		case D3: {
			abcdfg = 4;
			e = 5;
			break;
		}
	}
	PORTC = diplayData[abcdfg];
	if (diplayData[e] == 1) {
		IOPORT_SET_BIT(PORTB, PIN5);
	} else {
		IOPORT_RESET_BIT(PORTB, PIN5);
	}
}

#define ENABLE_D1() IOPORT_SET_BIT(PORTB, PIN3)
#define DISABLE_D1() IOPORT_RESET_BIT(PORTB, PIN3)
#define ENABLE_D2() IOPORT_SET_BIT(PORTB, PIN2)
#define DISABLE_D2() IOPORT_RESET_BIT(PORTB, PIN2)
#define ENABLE_D3() IOPORT_SET_BIT(PORTB, PIN1)
#define DISABLE_D3() IOPORT_RESET_BIT(PORTB, PIN1)

// Timer0 overflow handler
ISR(TIMER0_OVF_vect) {
	switch (currD) {
		case D3: {
			DISABLE_D3();
			currD = D1;
			if (currSpeed >= 100)
			{
				setDigit();
				ENABLE_D1();
			}
			break;
		}
		case D2: {
			DISABLE_D2();
			currD = D3;
			setDigit();
			ENABLE_D3();
			break;
		}
		case D1: {
			DISABLE_D1();
			currD = D2;
			if (currSpeed >= 10)
			{
				setDigit();
				ENABLE_D2();
			}
			break;
		}
	}
}

void InitInterrupts() {
	MCUCR = (1<<ISC00) | (1<<ISC01); // SPEED input 1 front
	MCUCR |= (1<<ISC10); // Button input 1-0 front
	GICR = (1<<INT0)|(1<<INT1); // Enable interrupts
}


void InitPorts() {
	// Work ports
	IOPORT_PIN_FOR_INPUT(PORTD, PIN2); // Speed input
	
	IOPORT_PIN_FOR_INPUT(PORTD, PIN3); // Button input
	
	// Segmet Indicator
	IOPORT_PIN_FOR_OUTPUT(PORTC, PIN0); // A
	IOPORT_PIN_FOR_OUTPUT(PORTC, PIN1); // B
	IOPORT_PIN_FOR_OUTPUT(PORTC, PIN2); // C
	IOPORT_PIN_FOR_OUTPUT(PORTC, PIN3); // D
	IOPORT_PIN_FOR_OUTPUT(PORTC, PIN4); // F
	IOPORT_PIN_FOR_OUTPUT(PORTC, PIN5); // G
	IOPORT_PIN_FOR_OUTPUT(PORTB, PIN5); // E
	IOPORT_PIN_FOR_OUTPUT(PORTB, PIN1); // D3
	IOPORT_PIN_FOR_OUTPUT(PORTB, PIN2); // D2
	IOPORT_PIN_FOR_OUTPUT(PORTB, PIN3); // D1
	
	// LED
	IOPORT_PIN_FOR_OUTPUT(PORTB, PIN0);
	
	// Audio
	IOPORT_PIN_FOR_OUTPUT(PORTD, PIN0);
	
	// Other ports
	IOPORT_PIN_FOR_INPUT(PORTD, PIN4);
	IOPORT_SET_BIT(PORTD, PIN4);
	IOPORT_PIN_FOR_INPUT(PORTD, PIN5);
	IOPORT_SET_BIT(PORTD, PIN5);
	IOPORT_PIN_FOR_INPUT(PORTD, PIN6);
	IOPORT_SET_BIT(PORTD, PIN6);
	IOPORT_PIN_FOR_INPUT(PORTB, PIN4);
	IOPORT_SET_BIT(PORTB, PIN4);
	IOPORT_PIN_FOR_INPUT(PORTD, PIN7);
	IOPORT_SET_BIT(PORTD, PIN7);
}


int main(void)
{
	InitPorts();
	
	// Initial display
	//**************************************
	diplayData[0] = digits[8] & 0x3f;
	diplayData[1] = digits[8] >> 6;
	
	ENABLE_D1();
	ENABLE_D2();
	ENABLE_D3();
	currD = D1;
	setDigit();
	
	_delay_ms(1000);
	
	AUDIO_ON();
	LED_ON();
	_delay_ms(100);
	AUDIO_OFF();
	LED_OFF();
	
	diplayData[0] = 0x0;
	diplayData[1] = 0x0;
		
	DISABLE_D1();
	DISABLE_D2();
	DISABLE_D3();
	currD = D2;
	setDigit();
	//**************************************
	// End initial display
	
	diplayData[4] = digits[0] & 0x3f;
	diplayData[5] = digits[0] >> 6;
	
	ENABLE_TIMER0_OVF_INTERRUPT();
	START_TIMER0();
	ENABLE_TIMER1_COMPA_INTERRUPT();
	OCR1A = COMPA_INCREMENT;
	START_TIMER1();
	if (GET_BUTTON_INPUT_SIGNAL() == 1) {
		speedCtrlEnabled = TRUE;
	}

	InitInterrupts();
	
	sei();
	while(1) {
		switch (cmd) {
			case CMD_CALC_SPEED: {
				unsigned int newSpeed = 0;
				if (sigCntTmp > 0) {
					newSpeed = sigCntTmp*1.1992;
				} else {
					newSpeed = 0;
				}
				
				if (newSpeed != currSpeed) {
					unsigned char d = 0;
					unsigned char code = 0;
					// Calc D1
					d = newSpeed / 100;
					code = digits[d];
					diplayData[0] = code & 0x3f;
					diplayData[1] = code >> 6;
					// Calc D2
					d = (newSpeed % 100) / 10;
					code = digits[d];
					diplayData[2] = code & 0x3f;
					diplayData[3] = code >> 6;
					// Calc D3
					d = newSpeed % 10;
					code = digits[d];
					diplayData[4] = code & 0x3f;
					diplayData[5] = code >> 6;
					
					currSpeed = newSpeed;
				}
			}
			cmd = CMD_NONE;
			break;
		}
	}
}
