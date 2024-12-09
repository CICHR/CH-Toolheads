/*****************************************************************************
*
* Copyright (C) 2003 Atmel Corporation
*
* @file USI_UART_config.h
* Compiler
* @date original 18-July-2002 by JLL
*       modified 02-Oct-2003 by LTA
*       adapted  08-Aug-2017 by AMO
*
* Support mail  : avr@atmel.com
*
* AppNote       : AVR307 - Half duplex UART using the USI Interface
* AVR target	: ATtiny26, ATtiny2313, ATmega169
* 				: ATtiny84 family
*
* Description   : Header file for USI_UART driver
*
****************************************************************************/

//********** USI UART Defines **********//

//#define SYSTEM_CLOCK             14745600
//#define SYSTEM_CLOCK             11059200
#define SYSTEM_CLOCK              8000000UL
//#define SYSTEM_CLOCK              7372800
//#define SYSTEM_CLOCK              3686400
//#define SYSTEM_CLOCK              2000000
//#define SYSTEM_CLOCK              1843200
//#define SYSTEM_CLOCK              1000000

//#define BAUDRATE                   115200
#define BAUDRATE                    57600
//#define BAUDRATE                    28800
//#define BAUDRATE                    19200
//#define BAUDRATE                    14400
//#define BAUDRATE                     9600
//#define BAUDRATE					   4800

//Use with 1MHz Clock rate
//#define TIMER_PRESCALER           1
//#define PRESCALEDIV				(1<<CS00)
// Use with 8MHz Clock rate
#define TIMER_PRESCALER				8
#define PRESCALECMD					(1<<CS01)

#define UART_RX_BUFFER_SIZE        16     /* 2,4,8,16,32,64,128 or 256 bytes */
#define UART_TX_BUFFER_SIZE        16

//********** USI UART Defines **********//
#define DATA_BITS                 8
#define START_BIT                 1
#define STOP_BIT                  1
#define HALF_FRAME                5

#define USI_COUNTER_MAX_COUNT     16
#define USI_COUNTER_SEED_TRANSMIT (USI_COUNTER_MAX_COUNT - HALF_FRAME)
#define INTERRUPT_STARTUP_DELAY   (0x11 / TIMER_PRESCALER)
#define TIMER0_SEED               (256 - ( (SYSTEM_CLOCK / BAUDRATE) / TIMER_PRESCALER )) // has round off error
// #define TIMER0_SEED               (256 - ( (SYSTEM_CLOCK / BAUDRATE) / TIMER_PRESCALER ))-2

#if ( (( (SYSTEM_CLOCK / BAUDRATE) / TIMER_PRESCALER ) * 3/2) > (256 - INTERRUPT_STARTUP_DELAY) )
    #define INITIAL_TIMER0_SEED       ( 256 - (( (SYSTEM_CLOCK / BAUDRATE) / TIMER_PRESCALER ) * 1/2) )
    #define USI_COUNTER_SEED_RECEIVE  ( USI_COUNTER_MAX_COUNT - (START_BIT + DATA_BITS) )
#else
    #define INITIAL_TIMER0_SEED       ( 256 - (( (SYSTEM_CLOCK / BAUDRATE) / TIMER_PRESCALER ) * 3/2) )
    #define USI_COUNTER_SEED_RECEIVE  (USI_COUNTER_MAX_COUNT - DATA_BITS)
#endif

#define UART_RX_BUFFER_MASK ( UART_RX_BUFFER_SIZE - 1 )
#if ( UART_RX_BUFFER_SIZE & UART_RX_BUFFER_MASK )
    #error RX buffer size is not a power of 2
#endif

#define UART_TX_BUFFER_MASK ( UART_TX_BUFFER_SIZE - 1 )
#if ( UART_TX_BUFFER_SIZE & UART_TX_BUFFER_MASK )
    #error TX buffer size is not a power of 2
#endif

/* General defines */
#define TRUE   1
#define FALSE  0

/*
//Portierung der Register- und Bitnamen auf ATmega168A
#ifdef __AVR_ATmega168__
	#define USART_RXC_vect USART_RX_vect
	#define TIMER2_COMP_vect TIMER2_COMPA_vect
	#define UCSRA UCSR0A
	#define UCSRB UCSR0B
	#define UCSRC UCSR0C
	#define UDRE UDRE0
	#define UDR UDR0
	#define RXEN RXEN0
	#define TXEN TXEN0
	#define RXCIE RXCIE0
	#define RXC RXC0
	#define UBRRH UBRR0H
	#define UBRRL UBRR0L
	#define UCSZ0 UCSZ00
	#define UCSZ1 UCSZ01

#endif

#ifdef __AVR_ATtiny84__||__AVR_ATtiny24A__
	#define TIFR	TIFR0
	#define TIMSK	TIMSK0
	#define PCIF	PCIF0
	#define PCIE	PCIE0
	#define PCMSK	PCMSK0
	#define	PSR0	PSR10
	#define USI_DDR		DDRA
	#define USI_OUTPUT	PORTA
	#define USI_INPUT	PINA
	#define USI_DI_PIN	PA6
	#define USI_DO_PIN	PA5
#endif
*/

#define TIFR	TIFR0
#define TIMSK	TIMSK0
#define PCIF	PCIF0
#define PCIE	PCIE0
#define PCMSK	PCMSK0
#define	PSR0	PSR10
#define USI_DDR		DDRA
#define USI_OUTPUT	PORTA
#define USI_INPUT	PINA
#define USI_DI_PIN	PA6
#define USI_DO_PIN	PA5


//********** AVR 307 USI_UART Prototypes **********//
// unsigned char Bit_Reverse( unsigned char );
void 		  USI_UART_init(void);
void          USI_UART_Flush_Buffers( void );
void          USI_UART_Initialise_Receiver( void );
void          USI_UART_Initialise_Transmitter( void );
void          USI_UART_Transmit_Byte( unsigned char );
unsigned char USI_UART_Receive_Byte( void );
unsigned char USI_UART_Data_In_Receive_Buffer( void );
void		  USI_UART_puts(char *str);		// transmit a string
void 		  USI_UART_showBits(int byte);	// show binary value
void		  USI_UART_showHex(int byte);

