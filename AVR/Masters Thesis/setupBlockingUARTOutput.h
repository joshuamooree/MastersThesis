#ifndef setupBlockingUARTOutput
#define	setupBlockingUARTOutput

#include <avr\interrupt.h>
#include <stdio.h>
#include <avr\io.h>

/*
 *  setupBlockingUARTOutput.h
 *
 *  Author: Joshua Moore, see note below
 *  Date:	September 2, 2006
 *
 *  Notes:	
 *		The data rate it will setup for is 38400 bps
 *		The AVR clock is assumed to be 8 MHz
 *		The UART registers are assumed to be the same as in a Mega 16	
 *
 *	Usage:
  *		Place setupOutputDevice	outside any function and before the main
 *		loop (if that is where initializeOutputDevice is)
 *		Place initializeOutputDevice before the beginning of the main loop
 *	
 *	
 *	Note: this code is not original:
 * 		Most this code comes from Atmel app note 306, UART2x.c.
 *		There is some code from the AVR-Libc (WinAVR) documentation.
 *		However, this code has been modified to make it easy to use in
 *		a program.
 */
		

#define setupOutputDevice									\
															\
															\
/* Prototypes */											\
static int TransmitByte( char c, FILE *stream );			\
															\
															\
/* write function */										\
															\
static int TransmitByte( char c, FILE *stream )				\
{															\
	if (c == '\n')											\
		TransmitByte('\r', stream);							\
															\
															\
	/* Wait for empty transmit buffer */					\
	while ( !(UCSRA & (1<<UDRE)) )							\
		; 													\
	/* Start transmission */		                		\
	UDR = c;												\
															\
	return 0; 												\
}															\
															\
static FILE _AVRStdout = FDEV_SETUP_STREAM(TransmitByte, NULL, _FDEV_SETUP_WRITE);


#define initializeOutputDevice         	\
				UBRRL = 52;            	\
									  	\
				UCSRA |= (1 << U2X);    \
				UCSRB |= (1 << TXEN); 	\
										\
										\
				stdout = &_AVRStdout;
				

#endif
