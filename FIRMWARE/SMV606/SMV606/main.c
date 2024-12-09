/*
 * SMV606.c
 *
 * Created: 05.03.2022 19:30:57
 * Author : cichy
 */ 
#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include "lib/USI_UART/USI_UART.h"
#include <string.h>

#define received_msg_size 4
#define which_head "HHD"
#define head_confirm "HOK"
#define head_out0_ON "H0I"
#define head_out0_OFF "H0O"

#define head_code "SMV606.g"
#define message_tool "FDM"		//MAX 10 Znaku

#define out0 PA7

void IO_init(void) {
	DDRA |= (1 << out0);  // Set PA7 as an output pin
	PORTA &= ~ (1 << out0);  // Set PA7 low
}



int main(void) {
	IO_init();
	USI_UART_init();		// set up pin directions, flush buffers, etc
	char received_msg[received_msg_size];
	uint8_t pos = 0;
	uint8_t received_byte;
	USI_UART_Initialise_Transmitter();
	_delay_ms(10);
	for(;;) {
		while(USI_UART_Data_In_Receive_Buffer()) {
			received_byte = USI_UART_Receive_Byte();
			if (received_byte == '\n') {
				received_msg[pos] = '\0';  // Null-terminate the string
				
				if (pos > 0 && received_msg[pos - 1] == '\r') {
					received_msg[pos - 1] = '\0';
				}
				
				if (pos == 0 && received_byte != 'H') {
					// If the first byte is not 'H', reset the buffer position
					pos = 0;
					continue;
				}
				
				if (strcmp(received_msg, which_head) == 0) {
					USI_UART_Transmit_Byte('\n');
					_delay_ms(5);
					USI_UART_puts("M98 P");
					USI_UART_Transmit_Byte(0x22);
					USI_UART_puts(head_code);
					USI_UART_Transmit_Byte(0x22);
					USI_UART_Transmit_Byte('\n');
					
					} else if (strcmp(received_msg, head_confirm) == 0) {
					USI_UART_Transmit_Byte('\n');
					_delay_ms(5);
					USI_UART_puts("M118 P0 L1 S");
					USI_UART_Transmit_Byte(0x22);
					USI_UART_puts(message_tool);
					USI_UART_Transmit_Byte(0x22);
					USI_UART_Transmit_Byte('\n');
					
					} else if (strcmp(received_msg, head_out0_ON) == 0) {
					PORTA |=  (1 << out0);  // Set PA7 high
					
					} else if (strcmp(received_msg, head_out0_OFF) == 0) {
					PORTA &= ~ (1 << out0);  // Set PA7 low
				}
				
				pos = 0;  // Reset buffer position
				
				} else {
				if (pos < sizeof(received_msg) - 1) {  // Prevent buffer overflow
					received_msg[pos++] = received_byte;
					} else {
					pos = 0;  // Reset buffer position
				}
			}
		}
	}
}