#include "busmanager.h"
#include "../common/common.h"
#include <stdlib.h>

#ifdef AVR
#define F_CPU 30000000UL
#include <util/delay.h>
#endif
/*
	 - Require address list, and were they are located.
	 Either keep track of who requires answer, or src/dest in message.
	 - Tasklist for message sending.
*/

#define BUSMANDEBUG		1

#if (BUSMANDEBUG)
 #define debprintf(a,l,...) printf_clr(a,__VA_ARGS__)
 //#define debprintf(a,l,...) printf(__VA_ARGS__)
#else
 #define debprintf(a,l,...)
#endif

//Busman need streams. You can have streams and fifo's, without uarts.

void busman_init(busman_t* man, uint8_t num_streams){
	//Address of this device:
	man->address = DEV_ADDRESS;

	//Create a bunch of stream pointers
	man->num_streams = num_streams;
	man->streams = (stream_t**)malloc(((int)num_streams)*sizeof(stream_t*));
	man->streaminfo = (streaminfo_t*)malloc(((int)num_streams)*sizeof(streaminfo_t));

	//Set them to nothing:
	for (uint8_t i=0;i<num_streams;i++){
		man->streams[i] = NULL;	
		man->streaminfo[i].addressmin = 0;	
		man->streaminfo[i].addressmax = 0;
	}

	//Setup message handlers:
	for (uint8_t i=0;i<8;i++){
		man->cmd_handler[i] = NULL;
		man->cmd_map[i] = 0;
	}
	

	debprintf(CLR_GREEN,0,"Busman inited\n");
}


void busman_setupstream(busman_t* man, uint8_t index, stream_t* stream){
	//Checking?

	//Attach
	man->streams[index] = stream;
}



net_error_e busman_sendmessage(busman_t* man, stream_t* stream, uint8_t* data, uint16_t len, uint8_t cmd,uint8_t addr){	
	if (!man){
		return NET_ERR_NULL;
	}
	if (!stream){
		return NET_ERR_NULL;
	}
	net_error_e err = message_append_tofifo(stream->txfifo,data,len,cmd,addr);
	//Write the message:
	stream->write_start(stream);
	return err;
}

//Parse a message on stream index's rxfifo.
net_error_e busman_parsemessage(busman_t* man,uint8_t index){
	
	net_error_e err;
	//Temp location for data:
	uint8_t msgdata[32];
	msg_info_t info;

	//We expect the same back:
	err = parse_message(man->streams[index]->rxfifo,msgdata,32,&info);
	if (err != NET_ERR_NONE){
		//Failed.
		return err;
	}else{
		//What should we do with the message?
		uint8_t cmd = (info.cmd & MASK_CMD);
		//Went ok.
		debprintf(CLR_GREEN,0,"Parse OK: addr %u cmd: %u\n",info.address,cmd);
		
		
		//Does a commandhadler exist?
		uint8_t handler_index;
		if (busman_gethandler(man,cmd,&handler_index)){
			return man->cmd_handler[handler_index](man,man->streams[index],msgdata,&info);
		}else{
			return busman_handlemessage(man,man->streams[index],msgdata,&info);
		}
		
	}
	return NET_ERR_UNDEFINED;

}

//Where the response should go
net_error_e busman_handlemessage(busman_t* man,stream_t* respstream,uint8_t* data,msg_info_t* info){

	bool ack = ((info->cmd & MASK_CMD_ACK) == MASK_CMD_ACK);
	uint8_t cmd = info->cmd & MASK_CMD;


	switch(cmd){
		case CMD_PING:			
			if (ack){
				debprintf(CLR_GREEN,0,"Received a ping response, len %u, data %u\n",info->datalen,data[0]);	
			}else{
				debprintf(CLR_GREEN,0,"Received a ping: len %u, responding...\n",info->datalen);	
				#ifdef AVR
				//Magic delay, with a FTDI, 1ms timeout and a 15ms read call.
				//Any faster, and it misses characters.
				//_delay_ms(1);
				#endif
				//Ping back
				return busman_sendmessage(/*man*/man,/*ifnum*/respstream,/*data*/data,/*len*/info->datalen,/*cmd*/CMD_PING|MASK_CMD_ACK,/*addr*/1);	
			}
			return NET_ERR_NONE;
		break;
		default:
			debprintf(CLR_RED,0,"Received unknown command %02X.\n",info->cmd);
			return NET_ERR_CMD_UNKOWN;

	}

	return NET_ERR_UNDEFINED;
}


void busman_printinfo(busman_t* man){
	debprintf(CLR_CANCEL,0,"Busmanager:\n");
	debprintf(CLR_CANCEL,0," streams: %u\n",man->num_streams);


	for (int i=0;i<man->num_streams;i++){
		debprintf(CLR_CANCEL,0,"Stream[%i]\n",i);
		stream_printinfo(man->streams[i]);
	}	
}

//Read messages.
void busman_task(busman_t* man){
	net_error_e err;
	//Look for messages:
	for (int i=0;i<man->num_streams;i++){		
		uint8_t num = messages_infifo(man->streams[i]->rxfifo);	
		debprintf(CLR_CANCEL,0,"%u messages in fifo\n",num );

		while((num = messages_infifo(man->streams[i]->rxfifo))){
			debprintf(CLR_GREEN,0,"There are %u messages in %i's rxfifo\n",num, i);
			//Do it
			err = busman_parsemessage(man,i);
			if (err != NET_ERR_NONE){
				debprintf(CLR_RED,0,"Failed to parse:");net_error(err);				
				//Continue				
			}else{
				debprintf(CLR_GREEN,0,"Parsed message\n");				
			}
		}
	}
}


bool busman_gethandler(busman_t* man,uint8_t cmd, uint8_t* index){

	if (cmd == 0){
		return false;
	}

	for (int i=0;i<8;i++){
		if (man->cmd_map[i] == cmd){
			//See if the funtion pointer is set:
			if (man->cmd_handler){
				//Set the index
				*index = i;
				return true;
			}
		}
	}
	return false;
}