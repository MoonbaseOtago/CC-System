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

#ifndef __IP_PACKET_INTERFACE
#define __IP_PACKET_INTERFACE

//
// Copyright 2013 Paul Campbell paul@taniwha.com
//

#include "packet_interface.h"

#ifdef __cplusplus
extern "C" {
#endif
extern rf_handle rf_open_ip(const char *serial_device, rf_rcv rcv_callback);
extern void rf_set_ip_enable(rf_handle handle, int on);
#ifdef __cplusplus
};

class ip_rf_interface: public rf_interface {
public:
	ip_rf_interface(const char *serial_device, rf_rcv rcv_callback);
	~ip_rf_interface();
	void set_ip_enable(bool on);
protected:
	suota_repeat	*repeat_list;
	typedef struct ip_active {
		struct ip_active *next;
		unsigned int	tag;
		int		fd;
		int		socket;
		bool		udp;		// true is UDP
		unsigned char	mac[8];
		bool		crypto;
		unsigned char	key;
		time_t		last_active;
	} ip_active;
	virtual bool data_receive(bool crypto, unsigned char key, unsigned char *mac, unsigned char *p, int len);
	static void *ip_listener_thread(void *);
	void ip_listener_thread();
	static void *ip_lookup_thread(void *);
	void ip_lookup_thread();
	ip_active	*ip_list;
	bool		ip_enabled;
	typedef struct ip_lookup {
		unsigned int		tag;
		struct ip_lookup	*next;
		char			name[128];
		unsigned char		mac[8];
		unsigned char		key;
		bool			crypto;
	} ip_lookup;
	int last_tag;
	int alloc_ip_tag();
	ip_lookup *ip_lookup_list;
	ip_lookup *ip_lookup_list_last;
	bool ip_lookup_busy;
	bool ip_listen_busy;
	virtual bool virtual_command(const char *cc, const char *file, int line);
};

#endif
#endif
