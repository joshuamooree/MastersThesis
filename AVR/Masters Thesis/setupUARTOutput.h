#ifndef setupUARTOutput
#define	setupUARTOutput

#include <avr\interrupt.h>
#include <stdio.h>
#include <avr\io.h>
#include <inttypes.h>

/*
 *  setupUARTOutput.h
 *
 *  Author: Joshua Moore, see note below
 *  Date:	September 2, 2006
 *
 *  Notes:	
 *		The data rate it will setup for is 19200 bps
 *		The AVR clock is assumed to be 8 MHz
 *		The UART registers are assumed to be the same as in a Mega 16	
 *
 *	Usage:
  *		Place setupOutputDevice	outside any function and before the main
 *		loop (if that is where initializeUART is)
 *		Place initializeOutputDevice before the beginning of the main loop
 *	
 *	
 *	Note: this code is not original:
 * 		Most this code comes from Atmel app note 306, UART2x.c.
 *		There is some code from the AVR-Libc (WinAVR) documentation.
 *		However, this code has been modified to make it easy to use in
 *		a program.
 */
		

/* UART Buffer Defines */     
#define UART_TX_BUFFER_SIZE 128 /* 2,4,8,16,32,64,128 or 256 bytes */


#define UART_TX_BUFFER_MASK ( UART_TX_BUFFER_SIZE - 1 )
#if ( UART_TX_BUFFER_SIZE & UART_TX_BUFFER_MASK )
	#error TX buffer size is not a power of 2
#endif



#define setupOutputDevice									\
															\
/* Static Variables */										\
volatile uint8_t UART_TxBuf[UART_TX_BUFFER_SIZE];			\
volatile uint8_t UART_TxHead;								\
volatile uint8_t UART_TxTail;								\
															\
/* Prototypes */											\
void InitUART( unsigned char baudrate );					\
static int TransmitByte( char c, FILE *stream );			\
															\
															\
ISR( USART_UDRE_vect )										\
{															\
	uint8_t tmptail;										\
															\
	/* Check if all data is transmitted */					\
	if ( UART_TxHead != UART_TxTail )						\
	{														\
		/* Calculate buffer index */						\
		tmptail = ( UART_TxTail + 1 ) & UART_TX_BUFFER_MASK;\
		UART_TxTail = tmptail;      /* Store new index */	\
															\
		UDR = UART_TxBuf[tmptail];  /* Start transition */	\
	}														\
	else													\
	{														\
		UCSRB &= ~(1<<UDRIE); /* Disable UDRE interrupt */	\
	}														\
}															\
															\
/* write function */										\
															\
static int TransmitByte( char c, FILE *stream )				\
{															\
	if (c == '\n')											\
		TransmitByte('\r', stream);							\
															\
	uint8_t tmphead;										\
	/* Calculate buffer index */							\
	tmphead = ( UART_TxHead + 1 ) & UART_TX_BUFFER_MASK;	\
	/* Wait for free space in buffer */						\
	while ( tmphead == UART_TxTail );						\
															\
	UART_TxBuf[tmphead] = c;  /* Store data in buffer */	\
	UART_TxHead = tmphead;       /* Store new index */		\
															\
	UCSRB |= (1<<UDRIE);        /* Enable UDRE interrupt */ \
															\
	return 0; 												\
}															\
															\
static FILE _AVRStdout = FDEV_SETUP_STREAM(TransmitByte, NULL, _FDEV_SETUP_WRITE);


#define initializeOutputDevice         	\
				UBRRL = 52;            	\
									  	\
				UCSRA |= (1 << U2X); 	\
				UCSRB |= (1 << TXEN); 	\
									  	\
									  	\
				UART_TxTail = 0;		\
				UART_TxHead = 0;		\
										\
				sei();					\
										\
				stdout = &_AVRStdout;
				

#endif
