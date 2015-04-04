#ifdef AVR
 //Atomic operations on fifo from ISR
 #include <util/atomic.h>
#else
 //On PC, just don't.
 #define ATOMIC_RESTORESTATE 
 #define ATOMIC_BLOCK(a) 
#endif

#include <stdio.h>
#include "fifo.h"

void fifo_init(fifo_t *fifo, volatile uint8_t *buffer, uint16_t size)
{
	fifo->buffer = buffer;
	fifo->size = size;	
	fifo->head = 0;
	fifo->tail = 0;
	fifo->free = size;
}

void fifo_clear(fifo_t *fifo)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
		fifo->head = 0;
		fifo->tail = 0;
		fifo->free = fifo->size;
	}	
}

//Appends data to the end of the fifo.
bool fifo_put(fifo_t *fifo, uint8_t *data)
{
	if (fifo_isfull(fifo))
		return false;
	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
		fifo->buffer[fifo->head] = *data;
		fifo->head = fifo->head + 1;
		//Wrap around to beginning if needed.
		if (fifo->head == fifo->size){
			fifo->head = 0;
		}		
		fifo->free--;
	}	
	return true;	
}

//Appends data to the end of the fifo.
bool fifo_putc(fifo_t *fifo, uint8_t data)
{
	if (fifo_isfull(fifo))
		return false;
	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
		fifo->buffer[fifo->head] = data;
		fifo->head = fifo->head + 1;
		//Wrap around to beginning if needed.
		if (fifo->head == fifo->size){
			fifo->head = 0;
		}		
		fifo->free--;
	}	
	return true;	
}

//Grabs data from the fifo.
bool fifo_get(fifo_t *fifo, uint8_t *data)
{
	if (fifo_isempty(fifo))
		return false;
	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
		*data = fifo->buffer[fifo->tail];
		fifo->tail = fifo->tail + 1;
		//Wrap around to beginning if needed.
		if (fifo->tail == fifo->size){
			fifo->tail = 0;
		}		
		fifo->free++;
	}	
	return true;	
}

//Peek at data from the fifo at offset from current head. 
//offs is used as tail pointer.
bool fifo_peek(fifo_t *fifo, uint8_t *data, uint16_t* offs){
	if (fifo_isempty(fifo))
		return false;
			
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
		uint16_t mem = *offs;
		if (fifo_used(fifo) <= mem){
			return false;
		}

		mem = fifo->tail + mem;
		if (mem >= fifo->size){
			mem -= fifo->size;
		}

		*data = fifo->buffer[mem];
		(*offs)++;
		
	}
	return true;
}

//Print fifo info
void fifo_print(fifo_t* fifo){
	printf("Fifo size=%i address:%p:\n",sizeof(fifo_t),fifo);
	printf(" head: %hu\n",fifo->head);
	printf(" tail: %hu\n",fifo->tail);
	printf(" size: %hu\n",fifo->size);
	printf(" free: %hu\n",fifo->free);
	printf(" buffer: %p %i\n",fifo->buffer,sizeof(fifo->buffer));
}