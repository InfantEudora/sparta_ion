#ifndef __LIB_FILE__
#define __LIB_FILE__

#include <stdio.h>
#include <stdbool.h>
#include "crc.h"

#include <dirent.h>

//Log file is just a generic file for appending data.
typedef struct logfile_t logfile_t;


struct logfile_t{
	FILE* file;
	char* name;
};


//HEX FILE used for loading AVR firmware.
typedef struct hex_file_s hex_file_s;

struct hex_file_s{
	
	int fd;                        //File descriptor.
	FILE* f;                        //File descriptor STREAM
	int filesize;
	int linecount;
	uint32_t binary_size;			//Size of memory footprint.
	uint16_t binary_crc;			//Computed CRC of binary.
	uint8_t data[128*1024];

    DIR *dp;                        //Directory descriptor.
    struct dirent *dptr;            //Dirent struct.
    uint8_t filename[256];           //Name for active log file.
    uint8_t directoryname[64];      //Name for active folder.	
};

int hexfile_open(hex_file_s*,char* file);
int hexfile_process(hex_file_s* file);
int parse_hexline(hex_file_s* file, uint8_t* buff,long offset,uint8_t* parsed_len);

#endif