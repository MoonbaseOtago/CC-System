// (c) Copyright Paul Campbell paul@taniwha.com 2013
#ifndef __suota_h_
#define __suota_h_
#include "interface.h"
void incoming_suota_packet(packet __xdata* p, u8 len);
u8 incoming_suota_version(packet __xdata *p);
void suota_setup();

typedef struct suota_req {
	unsigned char arch;
	unsigned char version[3];
	unsigned int  offset;
	u8	      id[4];
} suota_req;

typedef struct suota_resp {
	unsigned char arch;
	unsigned char version[3];
	unsigned int  offset;
	unsigned int  total_len;
	u8	      data[64];
} suota_resp;



extern code_hdr  __code * __xdata current_code;


#endif
