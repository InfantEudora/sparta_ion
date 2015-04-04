#ifndef __STREAM__
#define __STREAM__

#include <stdio.h>
#include <stdbool.h>
#include "fifo.h"
#include "uart.h"

//Forward declare it
typedef struct stream_t stream_t;
// Structures
struct stream_t{
	void (*write_start)(stream_t*);
	void (*write_stop)(stream_t*);
	fifo_t *rxfifo;
	fifo_t *txfifo;
	uart_s *uart;
};

//Read a single byte from stream's fifo into a buffer:
#define 	stream_readc(stream,data) stream_read(stream,data,1)

bool 		stream_init(stream_t *stream, uart_s* uart);
void 		stream_close(stream_t *stream);

//Read to a buffer:
uint16_t 	stream_read(stream_t *stream, uint8_t *data, uint16_t max);								//Calls read from buffer, returns instantly.
uint16_t 	stream_read_until(stream_t *stream, uint8_t *data, uint16_t max, uint8_t term);			
uint16_t 	stream_read_timeout(stream_t *stream, uint8_t *data, uint16_t max, uint16_t timeout);	

//Writing:
uint16_t 	stream_put(stream_t *stream, uint8_t *data, uint16_t size);			//Store a block of mem in txfifo
bool 		stream_putc(stream_t *stream, uint8_t data);						//Store a single character.
uint16_t 	stream_write(stream_t *stream, uint8_t* data, uint16_t size);		//stream put followed by write_start.

//Default write functions.
void stream_write_start(stream_t* stream);
void stream_write_stop(stream_t* stream);

//Test write functions.
void stream_writestart(stream_t* stream);
void stream_writestop(stream_t* stream);

//Copy functions.
uint16_t stream_tofifo(fifo_t* fifo, uint8_t *data, uint16_t size);				//Write to an unbound fifo.
void stream_rx_tostream_tx(stream_t *stream_in, stream_t *stream_out);			//Copy fifoin_rx to fifoout_tx.

//For keeping track of a pattern.
typedef struct pattern_state_t pattern_state_t;
struct pattern_state_t{
	uint8_t* pattern;
	uint8_t* match;
	uint8_t pattlen;	
	uint8_t matched; 	//Number of matches found.
	bool found;	
};

void pattern_init(pattern_state_t* pattern, uint8_t* pat, uint8_t len);

bool stream_rx_tostream_tx_pattern(stream_t *stream_in, stream_t *stream_out,pattern_state_t* patt);

//Info
void stream_printinfo(stream_t *stream);
#endif
