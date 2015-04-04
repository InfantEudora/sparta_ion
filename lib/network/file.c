#include "file.h"

#include "libdebug.h"

#define file_printf(a,l,...) printfdebug_file(a,l,__VA_ARGS__)


int hexfile_open(hex_file_s* file,char* fname){

	//Set the file descriptors.
	file->fd = 0;
	file->dp = NULL;
	file->dptr = NULL;
    // Buffer for storing the directory path
    memset(file->directoryname,0,sizeof(file->directoryname));

    //Copy the log path.
    strcpy((char*)file->directoryname,"/");

    // Open the directory stream
    if(NULL == (file->dp = opendir((const char*)file->directoryname))){
        file_printf(CLR_CANCEL,0,"Hexfile cannot cannot open input directory [%s].\n",file->directoryname);        
    }
		
	// Opens a logfile for output.	
	strncpy((char*)file->filename,fname,256);	
	
	//file->fd = open((char*)file->filename, O_RDONLY);
	file->f = fopen((char*)file->filename,"r");
		
	if(file->f == NULL){
		file_printf(CLR_CANCEL,0,"Unable to open hexfile ""%s"".\n",file->filename);
		//When file exists, 
		return 0;
	}else{
		file_printf(CLR_CANCEL,0,"Hexfile opened.\n");
		return 1;
	}
}

/*
	Parses intel HEX format in 16 byte lines
*/
int parse_hexline(hex_file_s* file, uint8_t* buff,long offset,uint8_t* parsed_len){
	buff +=1;
	uint8_t line_len;
	uint8_t mem[16];
	int res;
	unsigned long  chr;
	//Read line byte count:
	res = sscanf((char*)buff,"%2X",&chr);
	buff+=2;
	if (chr > 0x10){
		file_printf(CLR_CANCEL,0,"INTEL_HEX: Line longer than 16 bytes.\n");
		return 0;
	}

	line_len = chr;
	

	//Read address:
	uint16_t address;
	res = sscanf((char*)buff,"%4X",&address);

	file_printf(CLR_CANCEL,1,"INTEL_HEX: %2u bytes at %04X.\n",line_len,address);

	//Assume ascending, consecutive addresses.
	buff+=6;	
	for (uint8_t i =0;i<line_len;i++){
		res = sscanf((char*)buff,"%2X",&chr);
		if (res == 1){
			mem[i] = chr;
			buff+=2;			
		}else{
			file_printf(CLR_CANCEL,0,"Error parsing.\n");
			*parsed_len = 0;
			return 0;
		}
	}



	//Show em
	
	file_printf(CLR_CANCEL,1,"HEX:");
	for (uint8_t i=0;i<16;i++){
		file_printf(CLR_CANCEL,1," %02X",mem[i]);
	}
	file_printf(CLR_CANCEL,1,"\n");
	
	//Save:
	

	if (line_len!= 0x10){
		file_printf(CLR_CANCEL,1,"INTEL_HEX: Different line offset.\n");
		memcpy((uint8_t*)&file->data[offset],mem,line_len);
		*parsed_len = line_len;		
		//return 0;

	}else{
		//this only works if all lines except last is 16
		memcpy((uint8_t*)&file->data[offset],mem,line_len);
		*parsed_len = line_len;
	}

	return 1;
}

/*
	Parses the hexfile.
*/
int hexfile_process(hex_file_s* file){
	uint8_t buff[256*256];	
	uint32_t offs = 0;
	file->binary_size = 0;

	//Get filesize:
	//fseek(file->f, 0, SEEK_END); // seek to end of file
	//file->filesize = ftell(file->f); // get current file pointer
	//fseek(file->f, 0, SEEK_SET); // seek to end of file
	uint16_t s;
	while(1){
		s = fread(&buff[offs],1,256,file->f);
		offs += s;
		if (s==0){
			break;
		}
	}
	file->filesize = offs;
	file_printf(CLR_CANCEL,0,"HEX_PROCESS: HEX file read. filesize %lu\n",file->filesize);

	//Find the number of lines
	file->linecount = 0;
	uint8_t bytes_parsed = 0;
	//Memory offset.
	long offset = 0;
	for (uint32_t i=0;i<file->filesize;i++){
		if (buff[i] == ':'){
			
			if (!parse_hexline(file,&buff[i],offset,&bytes_parsed)){
				file->linecount++;
				file->binary_size+= bytes_parsed;
				offset += bytes_parsed;
				break;
			}
			file->linecount++;
			file->binary_size+= bytes_parsed;
			offset += bytes_parsed;
		}
	}

	//Fill blocks with 0s:
	int32_t  diff = file->binary_size;

	//Make the binary size round to 256 bytes:
	file->binary_size = ((file->binary_size>>8)+1)<<8;

	diff = file->binary_size - diff;

	file_printf(CLR_CANCEL,0,"HEX_PROCESS: Adding %li 0's \n",diff);
	//memset(&file->data[file->binary_size - diff],0,diff);


	file_printf(CLR_CANCEL,0,"HEX_PROCESS: Number of lines: %lu\n",file->linecount);
	file_printf(CLR_CANCEL,0,"HEX_PROCESS: Firmware Binary Size: %lu\n",file->binary_size);
	file->binary_crc = crc16_CCIT(file->data,file->binary_size,0xFFFF);

	file_printf(CLR_CANCEL,0,"HEX_PROCESS: Firmware CRC: %04X\n",file->binary_crc);

	//printf_hex_block((uint8_t*)&file->data[file->binary_size-512],512,1);

	//Close the file:
	fclose(file->fd);
	
	//Look for ':"
	return 1;
}