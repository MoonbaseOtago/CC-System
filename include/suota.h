// (c) Copyright Paul Campbell paul@taniwha.com 2013
// 
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) version 3, or any
// later version accepted by Paul Campbell , who shall
// act as a proxy defined in Section 6 of version 3 of the license.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public 
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//

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
	u8	      id[2];
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