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

#ifndef __protocol_h
#define __protocol_h

typedef struct packet {
	unsigned char	type;
	unsigned char	id[2];
	unsigned char	arch;
	unsigned char	version[3];
	unsigned char	data[1];
} packet;

typedef struct broadcast_filter {	// for playa protocol this is used as the first 3 bytes of every packet
	unsigned char	hops;
	unsigned char	uniq[2];	// uniq filter
} broadcast_filter;

#define P_TYPE_NOP		0
#define P_TYPE_SUOTA_REQ	1
#define P_TYPE_SUOTA_RESP	2

#define P_TYPE_OTHERS		0x40 // writing your own code, want to add your own code
			     	     // allocate something above here

#define THIS_ARCH	1	// initial CC2533
typedef struct code_hdr {
        unsigned char    crc[4];         
	unsigned char    arch;
        unsigned char    version[3];
        unsigned char    len[2];
        unsigned char    data[1];
} code_hdr;

#endif
