#include "message.h"

//Put a character in fifo with framing.
bool put_framechar(fifo_t* fifo,uint8_t data){
	if (data >= FRAME_ESCAPE_CHR){
		if (fifo_putc(fifo,FRAME_ESCAPE_CHR)){
			if (fifo_putc(fifo,data-FRAME_ESCAPE_CHR)){
				return true;
			}
			return false;			
		}
		return false;
	}else{
		return fifo_putc(fifo,data);
	}
	return false;
}

//Read a character, which shouldn't be a frame end or frame_start
net_error_e get_framechar(fifo_t* fifo, uint8_t* data){
	uint8_t chr;
	if (fifo_get(fifo,&chr)){
		if (chr == FRAME_ESCAPE_CHR){
			//Read another one,
			if (fifo_get(fifo,&chr)){
				//Should be 00-07
				if (chr > FRAME_ESCAPE_MAX){
					return NET_ERR_ESCAPE;
				}else{
					*data = (chr+FRAME_ESCAPE_CHR);
					return NET_ERR_NONE;
				}
			}else{
				return NET_ERR_NODATA;
			}
		}else if (chr > FRAME_ESCAPE_CHR){
			//That's all invalid in this state,
			return NET_ERR_ESCAPE;
		}else{			
			*data = chr;
			return NET_ERR_NONE;
		}
	}else{
		return NET_ERR_NODATA;
	}
	//Shouldn't get here
	return NET_ERR_UNDEFINED;
}


//Start a message in a fifo, info is used to update the datalen.
//Rolling CRC required.... somewhere.
net_error_e message_start(fifo_t* fifo, msg_info_t* info, uint8_t cmd, uint8_t address){

}

//Append data
net_error_e message_continue(fifo_t* fifo,msg_info_t* info, uint8_t* data, uint8_t chunk_size){

}

//Terminate message.
net_error_e message_stop(fifo_t* fifo,msg_info_t* info){

}


/*
	Append a message in the fifo, stores framing in fifo.
*/
net_error_e message_append_tofifo(fifo_t* fifo,uint8_t* data_in, uint8_t data_len,uint8_t cmd, uint8_t address){
	//Construct a low level message, append it to the fifo.
	uint8_t chr;
	//Compute CRC, update on every step. Computer over the unframed data,
	uint16_t crc = 0xFFFF;

	//Start frame:
	if (!put_framestart(fifo)){
		return NET_ERR_MEMORY;
	}
	
	//Protocol version:	
	chr = FRAME_PROT_VER;
	if (!put_framechar(fifo,chr)){
		return NET_ERR_MEMORY;
	}
	crc = crc16_CCIT(&chr,1,crc);
	
	//Address
	chr = address;
	if (!put_framechar(fifo,chr)){
		return NET_ERR_MEMORY;
	}
	crc = crc16_CCIT(&chr,1,crc);

	
	//command
	chr = cmd;
	if (!put_framechar(fifo,chr)){
		return NET_ERR_MEMORY;
	}
	crc = crc16_CCIT(&chr,1,crc);
	
	//Data len
	chr = data_len;
	if (!put_framechar(fifo,chr)){
		return NET_ERR_MEMORY;
	}
	crc = crc16_CCIT(&chr,1,crc);
	
	//Put the data in
	for (uint8_t i=0;i<data_len;i++){
		crc = crc16_CCIT(data_in,1,crc);
		if (!put_framechar(fifo,*data_in++)){
			return NET_ERR_MEMORY;
		}
	}
	
	//Store CRC:	
	chr = (crc>>8);
	if (!put_framechar(fifo,chr)){
		return NET_ERR_MEMORY;
	}	

	chr = (uint8_t)(crc);
	if (!put_framechar(fifo,chr)){
		return NET_ERR_MEMORY;
	}

	//Store end
	if (put_endframe(fifo)){
		return NET_ERR_NONE;
	}else{
		return NET_ERR_MEMORY;
	}

	return NET_ERR_UNDEFINED;
}

/*
	This function requires the following input:
		fifo:		fifo where the message is supposed to be stored. It will consume all characters before FRAME_START.
		data_max:	Maximum length of the data_out buffer.
	And the following output:
		msg_info_t	Holds info about the source of the message.
		data_out    Buffer which will hold the parsed data. No smaller than data_max.

	Message is deleted from fifo while parsing. 
	On error, data_out and src may contain invalid data. The fifo will be cleared until a new frame_start is found.

	TODO: Only data and CRC is de-framed.

*/
net_error_e parse_message(fifo_t* fifo,uint8_t* data_out, uint8_t data_max, msg_info_t* src){
	uint8_t chr;							//Temporary character.	
	uint8_t* data_out_start = data_out;		//Remeber beginning of data out.
	net_error_e err = NET_ERR_NODATA;		//If the while loop in the end fails, it will return this error.

	//Read from the fifo, until we find a start frame:
	while(fifo_get(fifo,&chr)){		
		if (chr == FRAME_START){
			break;	
		}
	}

	//Next character should be the protocol:	
	if (fifo_get(fifo,&chr)){
		if (chr != FRAME_PROT_VER){
			err = NET_ERR_PROT;
			goto end;
		}
	}

	//Get the adress.
	if (!(fifo_get(fifo,&chr))){
		return NET_ERR_NODATA;
	}else{
		src->address = chr;
	}

	//Read command
	if (!(fifo_get(fifo,&chr))){
		err = NET_ERR_NODATA;
		goto end;
	}else{
		src->cmd = chr;
	}	
	
	uint8_t msg_len;
	//Next should be the datalength.
	if (!(fifo_get(fifo,&chr))){
		err = NET_ERR_NODATA;
		goto end;
	}else{
		//We're not outputting it just yet.
		msg_len = chr;
	}

	//Assume this is correct, and read the data.	
	//Make sure it will fit in the temp. buffer.
	if (msg_len > data_max){
		//printf("Messagelen = %u\n",msg_len );
		return NET_ERR_MEMORY;
	}

	uint8_t i = msg_len;
	while(i--){
		err = get_framechar(fifo,&chr);
		if (err != NET_ERR_NONE){
			goto end;
		}
		*data_out++ = chr;
	}

	//Output len.
	src->datalen = msg_len;

	//Read the CRC from the message.
	uint16_t crc_msg;

	err = get_framechar(fifo,&chr);
	if (err != NET_ERR_NONE){
		goto end;
	}else{
		crc_msg = (chr << 8);
	}

	err = get_framechar(fifo,&chr);
	if (err != NET_ERR_NONE){
		goto end;
	}else{
		crc_msg |= (uint8_t)(chr);
	}

	//Calculate CRC the message should have:
	uint16_t crc_calc;

	chr = FRAME_PROT_VER;
	crc_calc = crc16_CCIT(&chr,1,0xFFFF);
	crc_calc = crc16_CCIT(&src->address,1,crc_calc);
	crc_calc = crc16_CCIT(&src->cmd,1,crc_calc);
	crc_calc = crc16_CCIT(&src->datalen ,1,crc_calc);
	crc_calc = crc16_CCIT(data_out_start,src->datalen,crc_calc);

	//Compare
	if (crc_calc != crc_msg){
		//printf_P(PSTR("CRC: %04X MSG: %04X"),crc_calc,crc_msg);
		err = NET_ERR_CRC;
		goto end;
	}

	//Read until the enc character is found.
	while(fifo_get(fifo,&chr)){
		if (chr == FRAME_END){
			err = NET_ERR_NONE;
			break;
		}
	}	
end:
	return err;
}

//Returns number of messages in fifo
uint8_t messages_infifo(fifo_t* fifo){
	if (!fifo){
		return 0;
	}

	uint16_t offs = 0;
	uint8_t chr;
	bool start = false;
	uint8_t cnt = 0;
	while(fifo_peek(fifo,&chr,&offs)){
		if (chr == FRAME_START){
			start = true;			
		}
		if (start && (chr == FRAME_END)){
			start = false;
			cnt++;
		}
	}
	return cnt;
}

//Error printing.
void net_error(net_error_e err){
	switch (err){
		case NET_ERR_NONE:
		printf("No error.\n");
		break;

		case NET_ERR_PROT:
		printf("NET_ERR_PROT\n");
		break;
		case NET_ERR_ADDRESS:
		printf("NET_ERR_ADDRESS\n");
		break;
		case NET_ERR_CMD_UNKOWN:
		printf("NET_ERR_CMD_UNKOWN\n");
		break;
		case NET_ERR_NODATA:
		printf("NET_ERR_NODATA\n");
		break;
		case NET_ERR_ESCAPE:
		printf("NET_ERR_ESCAPE\n");
		break;
		case NET_ERR_CRC:
		printf("NET_ERR_CRC\n");
		break;
		case NET_ERR_DATALEN:
		printf("NET_ERR_DATALEN\n");
		break;
		case NET_ERR_MEMORY:
		printf("NET_ERR_MEMORY\n");
		break;
		case NET_ERR_UNDEFINED:
		printf("NET_ERR_UNDEFINED\n");
		break;
		default:
		printf("Unknown error.\n");
		break;
	}	
}
