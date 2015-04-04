#ifndef _BUS_MESSAGE_H_
#define _BUS_MESSAGE_H_

/*
	RS-485/232/UART Network Layer Protocol.
	
	Multiple frame's can be put in the fifo.

	message_append_tofifo will fail if the fifo is full, and possibly leave part of a message.	

	Due to framing, the worst case length is 
	2 bytes header/footer
	4 bytes framing (We don't escape these...? TODO)
	4 bytes CRC
	2n bytes data.	

	When escaping is done in the ISR, only one message can fit in the fifo, but it's worst case size is:
	2 bytes header/footer
	4 bytes framing (We don't escape these...? TODO)
	2 bytes CRC
	n bytes data.	
	
	D.Prins - May 2014
*/

#include <stdint.h>

#include "fifo.h"
#include "crc.h"
#include "neterror.h"


//Frames
#define FRAME_START			0xFF
#define FRAME_END			0xFE
#define FRAME_PROT_VER		0x01
#define FRAME_ESCAPE_CHR	0xF8
#define FRAME_ESCAPE_MAX	0x07

//Offsets
#define OFFS_FRAME_PROT		0x00
#define OFFS_FRAME_ADDR		0x01
#define OFFS_FRAME_CMD		0x02
#define OFFS_FRAME_LEN		0x03
#define OFFS_FRAME_DATA		0x04

#define LEN_FRAME_HEAD		0x05
#define LEN_FRAME_CRC		0x02

#define MASK_CMD 			0x7F
#define MASK_CMD_ACK		0x80

//Macros:
#define put_framestart(fifo)	fifo_putc(fifo,FRAME_START)
#define put_endframe(fifo)		fifo_putc(fifo,FRAME_END)

/* Message format
	Offset  	Value 				Desc.
	0x00  		FRAME_START 		Start of message.
	0x01		FRAME_PROT_VER		01. Masked with 0x80 means message requires response.
	0x02		CMD 				Command can be 	
*/


/*
	Boot write format in data:
	data[0-1]: uint16_t block number
	data[2-3]: uint16_t offset within block.

	Data
*/


typedef enum{
	//Info read commands:
	CMD_GET_INFO			= 0x01,
	CMD_GET_SDCARD 			= 0x02,
	CMD_GET_EEPROM			= 0x03,
	
	CMD_BOOT_REQ			= 0x0A,			//Request to start bootloading.
	CMD_BOOT_START			= 0x0B,			//Command to start bootloading.
	CMD_BOOT_WRITE			= 0x0C,			//Write data 
	CMD_BOOT_CHECK			= 0x0D,			//Check chunk of data.
	CMD_BOOT_FINISH			= 0x0E,			//Finish bootloading.
	CMD_BOOT_INFO			= 0x0F,			//Get device boot information.

	CMD_PING				= 0x10,			//Pings devices
	CMD_TEST_DATA			= 0x11,			//Contains device specific test data.

	CMD_MEAS_READ			= 0x30,

	CMD_CTRL_SET			= 0x50,
	CMD_CTRL_READ			= 0x51,
	CMD_CTRL_STARTUP		= 0x53,

	CMD_CTRL_OFF			= 0x54,			//Control device on/off
	CMD_CTRL_PWR			= 0x55,			//Control device power levels.
	CMD_CTRL_NORMAL			= 0x56,			//Returns device to default state.
	

	CMD_CAL					= 0x60,

	CMD_SAVE				= 0x70,
	CMD_DATA				= 0x7F

}net_command_e;


typedef struct msg_info_t msg_info_t;

struct msg_info_t{
	uint8_t cmd;
	uint8_t address;
	uint8_t datalen;
	uint16_t crc;		//Used in start/continue/end
};


//Unescaped message
net_error_e make_message(fifo_t*,uint8_t* data_in, uint8_t data_len,uint8_t cmd, uint8_t address);

//Escaped message (framing)
net_error_e message_append_tofifo(fifo_t* fifo,uint8_t* data_in, uint8_t data_len,uint8_t cmd, uint8_t address);
net_error_e parse_message(fifo_t* fifo, uint8_t* data_out, uint8_t data_max, msg_info_t* info);

//Peeking:
uint8_t messages_infifo(fifo_t* fifo);

/*
	There are multiple ways of doing this. 
	We could escape the messages in the send routine. This means only one message can fit in the fifo
	at any time, making it memory inefficient for short messages.

	You'd need to use a queue with lenghts to seperate the message for the fifo.

	Another way, is storing them framed in the buffer. Then can simply be read one at a time.
	Either by receiving a flag uon reception of END frame, or by peeking in the fifo for an END frame.


*/

#endif

