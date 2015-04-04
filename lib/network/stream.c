#include "fifo.h"
#include "stream.h"

bool stream_init(stream_t *stream, uart_s* uart){
	//If you give it a uart, the uart's functions will be copied to stream.
	if (uart){
		//Copy the function pointers.
		stream->write_start = &stream_write_start;
		stream->write_stop = &stream_write_stop;
		//Copy fifo references:
		stream->rxfifo = uart->rxfifo;
		stream->txfifo = uart->txfifo;
	}else{
		//Else you must set them manually.	
		stream->rxfifo = NULL;
		stream->txfifo = NULL;
		//Clear the function pointers.
		stream->write_start = NULL;
		stream->write_stop = NULL;
	}	
	stream->uart = uart;	
	return true;
}


//Default the stream_writestart call uart's writestart
void stream_write_start(stream_t* stream){
	if (!stream)
		return;
	if (!stream->uart)
		return;
	if (stream->uart->write_start){
		#ifdef AVR
		stream->uart->write_start();
		#else
		stream->uart->write_start(stream->uart);
		#endif
	}
}

void stream_write_stop(stream_t* stream){
	if (!stream)
		return;
	if (!stream->uart)
		return;
	if (stream->uart->write_stop){
		#ifdef AVR
		stream->uart->write_stop();
		#else		
		stream->uart->write_stop(stream->uart);
		#endif
	}
}

//Or, you could create custom functions that print them to screen for instance.
void stream_writestart(stream_t* stream){
	printf("Started writing: ");

	if (!(stream->txfifo)){
		printf("Stream has no fifo.\n");
		return;
	}

	//Print all characters:
	uint8_t c;
	while(fifo_get(stream->txfifo,&c)){
		putchar(c);
	};

	//done.
	printf("\nDone.\n");
}

void stream_writestop(stream_t* stream){
	printf("Stopped writing.\n");
}


void stream_close(stream_t *stream){
	// Wait for TX buffer
	//	stream->writewait();

	// Clear FIFOs
	fifo_clear(stream->rxfifo);
	fifo_clear(stream->txfifo);
}

/*
	Read from stream to data with a max.
*/
uint16_t stream_read(stream_t *stream, uint8_t *data, uint16_t max){
	//Some safety
	if (!stream){
		return 0;
	}
	if (!stream->rxfifo){
		return 0;
	}




	//Start reading.
	uint16_t bytesread = 0;
	while ((bytesread < max) && fifo_get(stream->rxfifo, data)){
		data ++;
		bytesread ++;
	}	
	return bytesread;
}

/*
	Places all characters in it's fifo.
	When that's full, it will start writing and block until all data is copied out into the fifo.
*/
uint16_t stream_write(stream_t *stream, uint8_t *data, uint16_t size){	
	//Check if we have a fifo
	if (!stream->txfifo)
		return 0;

	uint16_t bytesleft = size;
	
	while (bytesleft--){
		if (fifo_isfull(stream->txfifo)){
			//Start writing it.
			if (stream->write_start){
				stream->write_start(stream);
			}else{
				return size-(bytesleft+1);
			}
		}
		//Block until free
		while (!fifo_put(stream->txfifo, data));
		//Next
		data ++;		
	}
	
	//Flush data out
	if (stream->write_start)
		stream->write_start(stream); // Notify
	
	return size;
}

//Load bytes into a fifo, without writing them out.
uint16_t stream_tofifo(fifo_t* fifo, uint8_t *data, uint16_t size){
	//Check if we have a fifo
	if (!fifo)
		return 0;

	uint16_t bytesleft = size;
	
	while (bytesleft--){
		if (!fifo_put(fifo, data)){
			return size-(bytesleft+1);
		}
		//Next
		data ++;		
	}
	
	return size;
}

//Write a single byte out.
bool stream_putc(stream_t *stream, uint8_t data){
	if (!fifo_isfull(stream->txfifo)){
		//Put it in.
		fifo_putc(stream->txfifo, data);
		if (stream->write_start){
			stream->write_start(stream); // Notify			
			return true;
		}else{
			//Can't clear fifo
			return false;
		}
		return false;
	}else{
		//Start writing and block:
		if (stream->write_start){
			stream->write_start(stream); // Notify
			while(!fifo_putc(stream->txfifo, data)); // Put character in FIFO
			return true;
		}else{
			//Can't clear fifo
			return false;
		}
	}
	return false;	
}

//Copy from stream's rx_fifo, to another stream's tx_fifo.
void stream_rx_tostream_tx(stream_t *stream_in, stream_t *stream_out){
	//Checks:
	if (!(stream_in && stream_out)){
		return;
	}
	if (!(stream_in->rxfifo && stream_out->txfifo)){
		return;
	}

	//Copy all data
	uint8_t ch;
	
	//Get characters
	while(fifo_get(stream_in->rxfifo,&ch)){
		//If they fit, put them in;
		if (!fifo_isfull(stream_out->txfifo)){
			fifo_putc(stream_out->txfifo,ch);
		}else{
			//Commence writing.
			if (stream_out->write_start){
				stream_out->write_start(stream_out);	
				//Block until it fits.				
				while (!fifo_putc(stream_out->txfifo,ch));
			}else{
				return;
			}
		}
	}
	//Start writing
	if (stream_out->write_start)
		stream_out->write_start(stream_out);
}

//Forward data and return true when a pattern has been found:
bool stream_rx_tostream_tx_pattern(stream_t *stream_in, stream_t *stream_out,pattern_state_t* patt){
	//Checks:
	if (!(stream_in && stream_out)){
		return false;
	}
	if (!(stream_in->rxfifo && stream_out->txfifo)){
		return false;
	}

	//Copy all data
	uint8_t ch;	
	
	//Get characters
	while(fifo_get(stream_in->rxfifo,&ch)){
		//Pattern matching
		if (ch == (*patt->match)){			
			patt->matched++;
			patt->match++;
			if (patt->matched == patt->pattlen){
				patt->found = true;
			}
		}else{
			//Reset
			patt->matched = 0;
			patt->match = patt->pattern;			
		}

		//If they fit, put them in;
		if (!fifo_isfull(stream_out->txfifo)){
			fifo_putc(stream_out->txfifo,ch);
		}else{
			//Commence writing.
			if (stream_out->write_start){
				stream_out->write_start(stream_out);	
				//Block until it fits.				
				while (!fifo_putc(stream_out->txfifo,ch));
			}else{
				return patt->found;
			}
		}
	}
	//Start writing
	if (stream_out->write_start)
		stream_out->write_start(stream_out);

	return patt->found;
}

void pattern_init(pattern_state_t* patt, uint8_t* pat, uint8_t len){
	patt->pattern = pat;
	patt->pattlen = len;
	patt->matched = 0;
	patt->match = patt->pattern;
	patt->found = false;
}

void stream_printinfo(stream_t *stream){
	printf("writestart: %p\n",stream->write_start);
	printf("writestop: %p\n",stream->write_stop);
	if (stream->rxfifo){
		printf("rxfifo:\n");
		fifo_print(stream->rxfifo);
	}else{
		printf("no rxfifo:\n");
	}
	if (stream->txfifo){
		printf("txfifo:\n");
		fifo_print(stream->txfifo);
	}else{
		printf("no txfifo:\n");
	}
}