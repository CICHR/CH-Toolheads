/**
 * @file USI_UART.c
 * This module contains the definitions and functions necessary to
 * implements a HW UART on a ATtiny using its Universal Serial Interface (USI).
 */
#define F_CPU 8000000UL

#include <avr/io.h>			// uController specific registers
#include <avr/interrupt.h>
#include <stdlib.h>			// for itoa() function
#include <util/delay.h>		// for debugging main()
#include "USI_UART.h"


//********** Static Variables **********//
// register unsigned char USI_UART_TxData asm("r15");	/* Tells the compiler
							/*	to use Register 15 instead of SRAM */
static volatile unsigned char USI_UART_TxData;	/* changed so that TxData
										is not over written on interrupt */
static unsigned char          UART_RxBuf[UART_RX_BUFFER_SIZE];
static volatile unsigned char UART_RxHead;
static volatile unsigned char UART_RxTail;
static unsigned char          UART_TxBuf[UART_TX_BUFFER_SIZE];
static volatile unsigned char UART_TxHead;
static volatile unsigned char UART_TxTail;

// Status byte holding flag definition, initialized to 0
static volatile union USI_UART_status {
    unsigned char status;
    struct {
        unsigned char ongoing_Transmission_From_Buffer:1;
        unsigned char ongoing_Transmission_Of_Package:1;
        unsigned char ongoing_Reception_Of_Package:1;
        unsigned char reception_Buffer_Overflow:1;
        unsigned char flag4:1;
        unsigned char flag5:1;
        unsigned char flag6:1;
        unsigned char flag7:1;
    };
} USI_UART_status = {0};

//********** USI_UART functions **********//

/**
 * Note that when the USI DO pin is configured as output, it is always
 * connected to the USIDR and takes on the value of the MSB.  Thus, when the
 * USIDR is not configured as a transmitter, the DO pin has to be forced to
 * a logical high in order to conform to the UART standard high-state idle
 * condition.  This state is implemented by configuring the DO pin as input
 * and then applying the internal pull-up resistors.
 */
void USI_UART_init(void) {
	// force the DO pin to the UART idle state (high)
    USI_DDR  &= ~((1<<USI_DI_PIN)|(1<<USI_DO_PIN));	// configure both as input
    USI_OUTPUT |= (1<<USI_DI_PIN)|(1<<USI_DO_PIN);	// Enable pull ups on both
    USI_UART_Flush_Buffers();				// set buffers at their beginnings
}

/**
 * Reverses the order of bits in a byte. (i.e. MSB is swapped with LSB, etc.)
 * @param x the byte whose bits need to be reversed: unsigned char
 * @return a byte with bits reversed in the same variable: unsigned char
 */
unsigned char Bit_Reverse( unsigned char x ) {
    x = ((x >> 1) & 0x55) | ((x << 1) & 0xaa);
    x = ((x >> 2) & 0x33) | ((x << 2) & 0xcc);
    x = ((x >> 4) & 0x0f) | ((x << 4) & 0xf0);
    return x;    
}

/**
 * Flush the UART buffers.
 * @param void
 * @return void
 */
void USI_UART_Flush_Buffers( void ) {
    UART_RxTail = 0;
    UART_RxHead = 0;
    UART_TxTail = 0;
    UART_TxHead = 0;
}

/**
 * Initialise USI for UART transmission.
 * @param void
 * @return void
 */
void USI_UART_Initialise_Transmitter( void ) {
    cli();									// disable all interrupts
    TCNT0  = 0x00;							//Zähler0 auf 0 setzen
    TIFR  |= (1<<TOV0);                      // Clear Timer0 OVF interrupt flag
    TIMSK |= (1<<TOIE0);                     // Enable Timer0 OVF interrupt
	TCCR0B |= (PRESCALECMD); 				/* Set prescaler to /8, start Timer0
						CS00 and CS02 need to be zero (default at startup) */
	GTCCR |= (1<<PSR0);						// start prescaller at 0
    USICR  = (0<<USISIE)|(1<<USIOIE)|       // Enable USI Counter OVF interrupt.
             (0<<USIWM1)|(1<<USIWM0)|       // Select Three Wire mode.
             // Select Timer0 overflow as USI Clock source.
             (0<<USICS1)|(1<<USICS0)|(0<<USICLK)|
             (0<<USITC);                                           
             
    USISR  = 0xF0 |              // Clear all USI interrupt flags.
             0x0F;				 /* Preload the USI counter to generate
             	 	 	 	 	 	 	 	   interrupt at first USI clock */
    USIDR  = 0xFF;               // Make sure MSB is '1' before enabling USI_DO
    USI_DDR  |= (1<<USI_DO_PIN); // Configure USI_DO as output
                  
    USI_UART_status.ongoing_Transmission_From_Buffer = TRUE;
                  
    sei();								// enable all interrupts
}

/** Initialise USI for UART reception.
 * Note that this function only enables pin change interrupt on the
 * USI Data Input pin. The USI is configured to read data within the
 * pinchange interrupt.
 * @param void
 * @return void
 * */
void USI_UART_Initialise_Receiver( void ) {  // is never used
	// place DO pin in UART in an idle state (high)
    USI_DDR &= ~(1<<USI_DO_PIN);		// Disconnect DO pin from USIDR
    USI_OUTPUT |= (1<<USI_DO_PIN);		//  and force to UART idle (high)
//    USI_DDR  &= ~(1<<USI_DI_PIN);		// config DI pin as input
//    USI_OUTPUT |= (1<<USI_DI_PIN);	// Enable pull up on USI DI
    USICR  =  0;                      // Disable USI.
    GIFR  |=  (1<<PCIF);              // Clear pin change (all) interrupt flag
    GIMSK |=  (1<<PCIE);              // Enable pin change interrupt for PORTB
    PCMSK |=  (1<<PCINT6);			  // Enable pin change interrupt for DI pin
}

/**
 * Puts data in the transmission buffer, after reversing the bits in the byte.
 * Initiates the transmission routines if not already started.
 * @param data a byte to be transmitted: unsigned char
 * @return void
 */
void USI_UART_Transmit_Byte( unsigned char data ) {
    unsigned char tmphead;
    // Calculate buffer index and if necessary, roll over at upper bound
    tmphead = (UART_TxHead + 1) & UART_TX_BUFFER_MASK;
    while ( tmphead == UART_TxTail );          // Wait for free space in buffer
    UART_TxBuf[tmphead] = Bit_Reverse(data);   /* Reverse the order of the bits
                                  in the data byte and store data in buffer */
    UART_TxHead = tmphead;                     // Store new index.
    // Start transmission from buffer (if not already started).
    if (!(USI_UART_status.ongoing_Transmission_From_Buffer)) {
        while (USI_UART_status.ongoing_Reception_Of_Package); /* Wait for USI
                                             to finish reading incoming data */
        USI_UART_Initialise_Transmitter();              
    }
}

/**
 * Returns a byte from the receive buffer. Waits if buffer is empty.
 * @param void
 * @return data from the buffer: unsigned char
 */
unsigned char USI_UART_Receive_Byte( void ) {
    unsigned char tmptail;
        
    while ( UART_RxHead == UART_RxTail );             // Wait for incoming data
    // Calculate buffer index and if necessary, roll over at upper bound
    tmptail = ( UART_RxTail + 1 ) & UART_RX_BUFFER_MASK;
    UART_RxTail = tmptail;                                // Store new index 
    return Bit_Reverse(UART_RxBuf[tmptail]);              /* Reverse the order
    	of the bits in the data byte before it returns data from the buffer */
}

/**
 * Check if there is data in the receive buffer.
 * @return  0 (FALSE) if the receive buffer is empty: unsigned char
 */
unsigned char USI_UART_Data_In_Receive_Buffer( void ) {
    return ( UART_RxHead != UART_RxTail );
}


// ********** Interrupt Handlers ********** //

/**
 * The pin change interrupt is used to detect USI_UART reception.
// It is here that the USI is configured to sample the UART signal.
 * @param PCINT0_vect
 */
ISR(PCINT0_vect) {
	/** The next code line is needed if more than one pin change interrupt
	 * is enabled in the PCINT0 group. If the USI DI pin is low, then it
	 * is likely this pin generated the pin change interrupt.  It sets up
	 * TIMER0 and the USI for the receive process.
	 */
    if (!(USI_INPUT & _BV(USI_DI_PIN) )) {  // USI_INPUT is PINA for ATtiny84
   /* Plant TIMER0 seed to match baudrate (including interrupt start up time)*/
        TCNT0  = INTERRUPT_STARTUP_DELAY + INITIAL_TIMER0_SEED;

        TCCR0B  |= (PRESCALECMD); 		/* Set prescaler to /8, and start Timer0
                    	CS00 and CS02 are set to zero at startup by default */
        GTCCR |= (1<<PSR0);					// start prescaller at zero
        TIFR  |= (1<<TOV0);                 // Clear Timer0 OVF interrupt flag
        TIMSK |= (1<<TOIE0);                // Enable Timer0 OVF interrupt
                                                                    
        USICR = (0<<USISIE)|(1<<USIOIE)|   // Enable USI Counter OVF interrupt
                 (0<<USIWM1)|(1<<USIWM0)|            // Select Three Wire mode.
                 (0<<USICS1)|(1<<USICS0)|(0<<USICLK)| // Select Timer0 OVR
                 (0<<USITC);                  //          as USI Clock source.
       // Note that enabling the USI will also disable the pin change interrupt.
        USISR  = 0xF0 |                       // Clear all USI interrupt flags.
                   USI_COUNTER_SEED_RECEIVE;  /* Preload the USI counter to
                   the number of bits to be shifted out before an interrupt */
        GIMSK &=  ~(1<<PCIE);   			// Disable pin change interrupts
        
        USI_UART_status.ongoing_Reception_Of_Package = TRUE;             
    }
}

/**
 * The USI Counter Overflow interrupt is used for moving data between memory
 * and the USI data register. The interrupt is used for both transmission
 * and reception; hence, its complexity.
 * @param USI_OVF_vect
 */
ISR(USI_OVF_vect) {
    unsigned char tmphead,tmptail;
    
    // Check if we are running in Transmit mode.
    if( USI_UART_status.ongoing_Transmission_From_Buffer ) {
        // If ongoing transmission, then send second half of transmit data.
        if( USI_UART_status.ongoing_Transmission_Of_Package ) {
        	// Clear on-going package transmission flag.
            USI_UART_status.ongoing_Transmission_Of_Package = FALSE;
            // Load USI Counter seed and clear all USI flags.
            USISR = 0xF0 | (USI_COUNTER_SEED_TRANSMIT);
            // Reload the USIDR with the rest of the data and a stop-bit.
            USIDR = (USI_UART_TxData << 3) | 0x07;
        }
        // Else start sending more data or leave transmit mode.
        else {
       // If there is data in the transmit buffer, then send first half of data
            if ( UART_TxHead != UART_TxTail )  {
            	// Set on-going package transmission flag.
                USI_UART_status.ongoing_Transmission_Of_Package = TRUE;
                // Calculate buffer index and if necessary, roll over.
                tmptail = ( UART_TxTail + 1 ) & UART_TX_BUFFER_MASK;
                UART_TxTail = tmptail;                       // Store new index
                /* Read out the data that is to be sent. Note that the data must
                   be bit reversed before sent. The bit reversing is moved to
                   the application section to save time within the interrupt */
                USI_UART_TxData = UART_TxBuf[tmptail];
                /* Load USI Counter seed and clear all USI flags */
                USISR  = 0xF0 | (USI_COUNTER_SEED_TRANSMIT);
                /* Copy (initial high state,) start-bit and 6 LSB of original
                 *                 data (6 MSB of bit of bit reversed data). */
                USIDR  = (USI_UART_TxData >> 2) | 0x80;
            }
            // Else enter receive mode.
            else {
            	USI_UART_status.ongoing_Transmission_From_Buffer = FALSE;
                TCCR0B  &= ~(PRESCALECMD);			// Stop Timer0
                USI_DDR |= _BV(USI_DO_PIN);			// config DO pin as output
//               USI_DDR  &= ~(1<<USI_DI_PIN);		// config DI pin as input
//               USI_OUTPUT |= (1<<USI_DI_PIN);	// Enable pull up on USI DI
                USICR  =  0;                        // Disable USI
                GIFR   |=  (1<<PCIF);        // Clear pin change interrupt flag
                GIMSK |=  (1<<PCIE);   	// Enable all pin change interrupts
 			    PCMSK |=  (1<<PCINT6);	 // Enable pin change interrupt for PA6
           }
        }
    }
    
    // Else running in receive mode.
    else {
        USI_UART_status.ongoing_Reception_Of_Package = FALSE;
        //Calculate buffer index and if necessary, roll over at upper bound.
        tmphead     = ( UART_RxHead + 1 ) & UART_RX_BUFFER_MASK;
        // If buffer is full trash data and set buffer full flag.
        if ( tmphead == UART_RxTail ) {
        	// Store status to take actions elsewhere in the application code
            USI_UART_status.reception_Buffer_Overflow = TRUE;
        }
        else {          // If there is space in the buffer then store the data.
            UART_RxHead = tmphead;                          // Store new index.
            /* Store received data in buffer. Note that the data must be bit
             * reversed before used.  The bit reversing is moved to the
             * application section to save time within the interrupt. */
            UART_RxBuf[tmphead] = USIDR;
//            UART_RxBuf(tmphead) = USIRB; // ? use the buffered USI register?
        }
        TCCR0B  &= ~(PRESCALECMD);  			// Stop Timer0.
//        USI_DDR  &= ~(1<<USI_DI_PIN);			// Set DI pin as input
//        USI_OUTPUT |=  (1<<USI_DI_PIN);		// Enable pull up on USI DI
        USI_DDR  |= (1<<USI_DO_PIN);			// Set DO pin as output
        USICR  =  0;                     		// Disable USI.
        GIFR  |=  (1<<PCIF);           // Clear pin change interrupt flag.
        GIMSK |=  (1<<PCIE);		   // Enable pin change interrupt
	    PCMSK |=  (1<<PCINT6);		   // Enable pin change interrupt for PA6
     }
    
}

/**
 * Timer0 Overflow interrupt is used to trigger the sampling of signals on
 * the USI Input pin - hopefully at midpoint in the bit period.
 * @param TIM0_OVF_vect
 */
ISR(TIM0_OVF_vect) {
    TCNT0 += TIMER0_SEED;    /* Reload the timer, current count is added for
    							timing correction */
}

/**
 * Send a string of characters
 * @param str - pointer to the string location: i.e. the array's name
 */
void USI_UART_puts(char *str) {
	uint8_t i = 0;
	while (str[i]) {		//Null char looks like a FALSE indication
		USI_UART_Transmit_Byte(str[i++]);
	}
}

void USI_UART_showBits(int byte) {
	char buf[17];
	itoa(byte, buf, 2);
	USI_UART_puts("0b");
	USI_UART_puts(buf);
}

void USI_UART_showHex(int byte) {
	char buf[17];
	itoa(byte, buf, 16);
	USI_UART_puts("0x");
	USI_UART_puts(buf);
}
