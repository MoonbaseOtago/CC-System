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
	unsigned char	arch;
	unsigned char	code_base;
	unsigned char	version[2];
	unsigned char	data[1];
} packet;

typedef struct broadcast_filter {	// for playa protocol this is used as the first 3 bytes of every packet
	unsigned char	hops;
	unsigned char	uniq[2];	// uniq filter
} broadcast_filter;

#define P_TYPE_NOP		0
#define P_TYPE_SUOTA_REQ	1
#define P_TYPE_SUOTA_RESP	2
#define P_TYPE_IP_LOW		3
#define	P_TYPE_IP_LOOKUP	4
#define	P_TYPE_IP_LOOKUP_RESP	5
#define	P_TYPE_IP_CONNECT_TCP	6
#define	P_TYPE_IP_OPEN_UDP	7
#define	P_TYPE_IP_OPEN_RESP	8
#define	P_TYPE_IP_CLOSE		9
#define	P_TYPE_IP_CLOSE_RESP	10
#define	P_TYPE_IP_SEND		11
#define	P_TYPE_IP_RECV		12
#define P_TYPE_IP_HIGH		12
#define P_TYPE_MANUFACTURING_TEST 0x3f

#define P_TYPE_OTHERS		0x40 // writing your own code, want to add your own code
			     	     // allocate something above here

#ifndef THIS_ARCH
#define THIS_ARCH	1	// initial CC2533	// 0 unavailable as an extension
#endif
#ifndef THIS_CODE_BASE
#define THIS_CODE_BASE	0	// initial CC2533
#endif
typedef struct code_hdr {	// caution: these offsets are hard coded in suota assembly, fixcrc and packet_interface.cpp
        unsigned char    crc[4];         
        unsigned char    len[2];
	unsigned char    arch;
        unsigned char    code_base;
        unsigned char    version[2];
        unsigned char    data[1];
} code_hdr;

#endif
