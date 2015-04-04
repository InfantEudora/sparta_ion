/*
 * busmanager.h
 *
 * Created: 6/13/2014 3:04:20 PM
 *  Author: Dick
 */ 
#ifndef __BUSMANAGER_H_
#define __BUSMANAGER_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "stream.h"
#include "message.h"


//This device's address.
#define DEV_ADDRESS 	0


//What type of device do I implement?
#define MANTYPE_ROUTER	1	//Listen to messages, and route them.
#define MANTYPE_MASTER	2	//Send messages by myself.
#define MANTYPE_SLAVE	4	//Respond to messages

//What do we implement:
#define MANTYPE 			MANTYPE_ROUTER|MANTYPE_MASTER|MANTYPE_SLAVE

typedef struct {
	//Addresses on this stream.
	uint8_t addressmin;
	uint8_t addressmax;
}streaminfo_t ;



typedef struct busman_t busman_t;

//Busmanager can handle multiple streams, forward messages.
struct busman_t{
	uint8_t address;
	uint8_t num_streams;
	stream_t** streams;	//List of steams.
	streaminfo_t* streaminfo;	//List of info about those streams.

	//Handlers for specific message types?	
	net_error_e (*cmd_handler[8])(busman_t* man,stream_t* respstream,uint8_t* data,msg_info_t* info);
	uint8_t cmd_map[8]; //Maps cmd types to handlers.

	//Info about addresses
};



void busman_init(busman_t* man, uint8_t numstreams);
void busman_setupstream(busman_t* man, uint8_t index, stream_t* stream);

net_error_e busman_parsemessage(busman_t* man,uint8_t index);
net_error_e busman_handlemessage(busman_t* man,stream_t* respstream,uint8_t* data,msg_info_t* info);


net_error_e busman_sendmessage(busman_t* man,stream_t* stream, uint8_t* data, uint16_t len, uint8_t cmd,uint8_t addr);

void busman_printinfo(busman_t* man);
void busman_task(busman_t* man);

//return true if the index is valid.
bool busman_gethandler(busman_t* man,uint8_t cmd, uint8_t* index);

#endif /* BUSMANAGER_H_ */