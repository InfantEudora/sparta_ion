#ifndef __FIFO__
#define __FIFO__

#include <stdint.h>
#include <stdbool.h>

//Fifo uses buffer as a cyclic buffer.
typedef volatile struct{
	volatile uint16_t head;
	volatile uint16_t tail;
	volatile uint16_t size;
	volatile uint16_t free;	
	volatile uint8_t *buffer;
}fifo_t; 

#define fifo_isfull(fifo) 		((fifo)->free == 0)
#define fifo_isempty(fifo) 		((fifo)->free == (fifo)->size)
#define fifo_free(fifo)			((fifo)->free)
#define fifo_used(fifo)			((fifo)->size - (fifo)->free)
#define fifo_available(fifo)	((fifo)->free)
#define fifo_size(fifo)			((fifo)->size)

void fifo_init(fifo_t *fifo, volatile uint8_t *buffer, uint16_t size);
void fifo_clear(fifo_t *fifo);
bool fifo_put(fifo_t *fifo, uint8_t *data);
bool fifo_putc(fifo_t *fifo, uint8_t data);
bool fifo_get(fifo_t *fifo, uint8_t *data);
bool fifo_peek(fifo_t *fifo, uint8_t *data, uint16_t* offs);

//Info
void fifo_print(fifo_t* fifo);

#endif
