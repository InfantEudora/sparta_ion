#ifndef _NET_ERROR_H_
#define _NET_ERROR_H_

#include <stdio.h>
//Enum for the different things that can go wrong.

typedef enum{
	NET_ERR_NONE			= 0x00,
	NET_ERR_PROT			= 0x01,
	NET_ERR_ADDRESS			= 0x02,
	NET_ERR_CMD_UNKOWN		= 0x03,
	NET_ERR_NODATA			= 0x04,
	NET_ERR_ESCAPE			= 0x05,
	NET_ERR_CRC				= 0x06,
	NET_ERR_DATALEN			= 0x07,
	NET_ERR_MEMORY			= 0x08,
	NET_ERR_NULL			= 0x09,
	NET_ERR_STATE			= 0x0A,
	NET_ERR_UNDEFINED		= 0xFF
}net_error_e;

void net_error(net_error_e err);

#endif
