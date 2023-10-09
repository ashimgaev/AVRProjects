#ifndef IO_PORTS_H
#define IO_PORTS_H

#include <avr/io.h>


#define PORTBDDRx DDRB
#define PORTCDDRx DDRC
#define PORTDDDRx DDRD

#define IOPORT_FOR_INPUT(port) port##DDRx=0x00
#define IOPORT_FOR_OUTPUT(port) port##DDRx=0xFF

#define IOPORT_PIN_FOR_INPUT(port,pin)	port##DDRx&=~(1<<pin)
#define IOPORT_PIN_FOR_OUTPUT(port,pin)	port##DDRx|=(1<<pin)


#define PIN0	0
#define PIN1	1
#define PIN2	2
#define PIN3	3
#define PIN4	4
#define PIN5	5
#define PIN6	6
#define PIN7	7



/*#############################################################

                    OUT PORTS

#############################################################*/

/* Sets 1 for pin for out port
port - port id PORTA, PORTB, PORTC, PORTD
pin - pin number PIN0/PIN1/PIN2/PIN3/PIN4/PIN5/PIN6/PIN7
*/
#define IOPORT_SET_BIT(port,pin)		 port|=(1<<pin)
/* Sets 0 for pin for out port
port - port id PORTA, PORTB, PORTC, PORTD
pin - pin number PIN0/PIN1/PIN2/PIN3/PIN4/PIN5/PIN6/PIN7
*/
#define IOPORT_RESET_BIT(port,pin)		 port&=~(1<<pin)



/* Sets 1 for all pins for out port
port - port id PORTA, PORTB, PORTC, PORTD
*/
#define IOPORT_SET_ALL_BITS(port)		 port=0xFF
/* Sets 0 for all pins for out port
port - port id PORTA, PORTB, PORTC, PORTD
*/
#define IOPORT_RESET_ALL_BITS(port)		 port=0x00


/*#############################################################

                    INPUT PORTS

#############################################################*/


#define PORTBPINx PINB

/* Returns a bit value on pin on input port
port - port id PORTA, PORTB, PORTC, PORTD
pin - pin number: PIN0/PIN1/PIN2/PIN3/PIN4/PIN5/PIN6/PIN7
*/
#define IOPORT_GET_BIT(port,pin) ((port##PINx & (1<<pin)) == 0 ? 0: 1)


/* Returns all bits on input port
port - port id PORTA, PORTB, PORTC, PORTD
*/
#define IOPORT_GET_ALL_BITS(port)	port##PINx

#endif
