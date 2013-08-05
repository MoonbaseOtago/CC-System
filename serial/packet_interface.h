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

#ifndef __PACKET_INTERFACE
#define PACKET_INTERFACE

//
// Copyright 2013 Paul Campbell paul@taniwha.com
//

#define PKT_MAGIC_0 0x93
#define PKT_MAGIC_1 0xa5

#define PKT_CMD_OK				0x00
#define PKT_CMD_RCV_OFF				0x01
#define PKT_CMD_RCV_ON				0x02
#define PKT_CMD_SET_CHANNEL			0x03
#define PKT_CMD_SET_KEY				0x04
#define PKT_CMD_RCV_PACKET			0x05
#define PKT_CMD_SET_MAC				0x06
#define PKT_CMD_SEND_PACKET			0x07
#define PKT_CMD_SEND_PACKET_MAC			0x08
#define PKT_CMD_PRINTF				0x09
#define PKT_CMD_PING				0x0a
#define PKT_CMD_SET_SUOTA_VERSION		0x0b
#define PKT_CMD_RCV_PACKET_BROADCAST		0x0c
#define	PKT_CMD_SET_PROMISCUOUS			0x0d
#define	PKT_CMD_SET_RAW				0x0e
#define PKT_CMD_SEND_PACKET_CRYPT		0x20	// 0x20->0x27 depending on key
#define PKT_CMD_SEND_PACKET_CRYPT_MAC		0x40	// 0x20->0x27 depending on key
#define PKT_CMD_RCV_PACKET_CRYPT		0x60	// only 1 key for now
#define PKT_CMD_RCV_PACKET_CRYPT_BROADCAST	0x80	// only 1 key for now

#ifndef SDCC
//
//	Packet layout
//
//	0:	PKT_MAGIC_0
//	1:	PKT_MAGIC_1
//	2:	PKT_CMD_*
//	3:	length (of data) not including CS
//	4:	data
//	N:	CS_hi
//	N+1:	CS_lo		16-bit sum	from byte 2 to byte N-1
//

//
//	PKT_CMD_OK	no data
//	PKT_CMD_RCV_OFF	no data
//
//	PKT_CMD_RCV_ON	
//		2:	key
//
//	PKT_CMD_SET_CHANNEL
//		2:	channel 11-?
//
//	PKT_CMD_SET_KEY
//		2	key
//		3-18:	aes key
//
//	PKT_CMD_RCV_PACKET*
//		2-9:	src mac
//		10-N-1:	data
//
//	PKT_CMD_SET_MAC:
//
//		2-9:	mac adddress (64-bit)
//
//	PKT_CMD_SEND_PACKET*
//		2-N-1:	data
//
//	PKT_CMD_SET_SUOTA_VERSION
//		2	arch
//		2-4	version
//

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <pthread.h>
typedef void *rf_handle;
typedef void (*rf_rcv)(rf_handle, int broadcast, int crypt, unsigned char *mac, unsigned char *data, int len);
extern rf_handle rf_open(const char *serial_device, rf_rcv rcv_callback);
extern void rf_close(rf_handle handle);
#define RF_NO_KEY (-1)
extern int rf_command(rf_handle handle, const char *cmd);
extern int rf_initialise(rf_handle handle, const char *file);
inline int rf_initialize(rf_handle handle, const char *file) { return rf_initialize(handle, file); }
extern void rf_on(rf_handle handle, int key);
extern void rf_off(rf_handle handle);
extern void rf_set_auto_dump(rf_handle handle, FILE *output);
extern void rf_set_channel(rf_handle handle, int channel);
extern void rf_set_key(rf_handle handle, int k, const unsigned char *key);
extern void rf_set_mac(rf_handle handle, const unsigned char *mac);
extern void rf_set_promiscuous(rf_handle handle, int on);
extern void rf_set_raw(rf_handle handle, int on);
extern void rf_send(rf_handle handle, const unsigned char *mac, const unsigned char *data, int len);
extern void rf_send_crypto(rf_handle handle, int key, const unsigned char *mac, const unsigned char *data, int len);
extern void rf_set_suota_version(rf_handle handle, unsigned char arch, unsigned long version);
extern int rf_set_suota_upload(rf_handle handle, int key, unsigned char arch, unsigned long version, const char *file);

#ifdef __cplusplus

class rf_interface {
public:
	rf_interface(const char *serial_device, rf_rcv rcv_callback);
	~rf_interface();
	void on(int key);
	void off();
	void ping();
	void set_auto_dump(FILE *output);
	void set_channel(int channel);
	void set_key(int k, const unsigned char *key);
	void set_mac(const unsigned char *mac);
	void set_promiscuous(int on);
	void set_raw(int on);
	void send(const unsigned char *mac, const unsigned char *data, int len);
	void send_crypto(int key, const unsigned char *mac, const unsigned char *data, int len);
	int opened_ok() { return fd >= 0; }
	int initialise(const char *file);
	int initialize(const char *file) { return initialise(file);} 
	int command(const char *cmd) { return command(cmd, 0, 0); }
	void set_suota_version(unsigned char arch, unsigned long version);
	int set_suota_upload(int key, unsigned char arch, unsigned long version, const char *file);
private:
	void	send_packet(int cmd, int, const unsigned char *);
	static void *thread(void *);
	void 	rf_thread();
	int	fd;
	bool	shutdown;
	rf_rcv 	callback;
	FILE	*auto_dump;
	pthread_mutex_t	mutex;
	pthread_cond_t	cond;
	pthread_t	tid;
	int command(const char *cmd, const char *file, int line);
	typedef struct suota_upload {
		struct suota_upload *next;
		unsigned char key;
		unsigned char arch;
		unsigned long version;
		unsigned char *data;
		unsigned long len;
	
	} suota_upload;
	unsigned char rf_id[2];
	suota_upload	*suota_list;

};

};
#endif
#endif

#endif
	
