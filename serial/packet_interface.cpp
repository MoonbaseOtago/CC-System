//
// Copyright 2013 Paul Campbell paul@taniwha.com
//
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
#include "packet_interface.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

typedef void *rf_handle;
typedef void (*rf_rcv)(rf_handle, int crypt, unsigned char *mac, unsigned char *data, int len);
rf_handle
rf_open(const char *serial_device, rf_rcv rcv_callback)
{
	rf_interface *i = new rf_interface(serial_device, rcv_callback);
	if (!i->opened_ok()) {
		delete i;
		return 0;
	}
	return (rf_handle)i;
}

void
rf_close(rf_handle handle)
{
	delete (rf_interface*)handle;
}

void
rf_on(rf_handle handle, int key)
{
	rf_interface *i = (rf_interface*)handle;
	i->on(key);
}

void
rf_off(rf_handle handle)
{
	rf_interface *i = (rf_interface*)handle;
	i->off();
}

void
rf_set_auto_dump(rf_handle handle, FILE *output)
{
	rf_interface *i = (rf_interface*)handle;
	i->set_auto_dump(output);
}

void
rf_set_channel(rf_handle handle, int channel)
{
	rf_interface *i = (rf_interface*)handle;
	i->set_channel(channel);
}

void
rf_set_key(rf_handle handle, int k, const unsigned char *key)
{
	rf_interface *i = (rf_interface*)handle;
	i->set_key(k, key);
}

void
rf_set_mac(rf_handle handle, const unsigned char *mac)
{
	rf_interface *i = (rf_interface*)handle;
	i->set_mac(mac);
}

void
rf_send(rf_handle handle, const unsigned char *mac, const unsigned char *data, int len)
{
	rf_interface *i = (rf_interface*)handle;
	i->send(mac, data, len);
}

void
rf_send_crypto(rf_handle handle, int key, const unsigned char *mac, const unsigned char *data, int len)
{
	rf_interface *i = (rf_interface*)handle;
	i->send_crypto(key, mac, data, len);
}


rf_interface::rf_interface(const char *serial_device, rf_rcv rcv_callback)
{
	struct termios tm;

	fd = ::open(serial_device, O_RDWR);
	if (fd < 0)
		return;
	tcgetattr(fd, &tm);
	cfmakeraw(&tm);
	cfsetspeed(&tm, B9600);
	tcsetattr(fd, TCSAFLUSH, &tm);
	callback = rcv_callback;
	shutdown = 0;
	auto_dump = 0;
	pthread_mutex_init(&mutex, 0);
	pthread_cond_init(&cond, 0);
	pthread_create(&tid, 0, thread, this);
}

rf_interface::~rf_interface()
{
	if (fd < 0)
		return;
	pthread_mutex_lock(&mutex);
	shutdown = 1;
	pthread_cond_broadcast(&cond);
	while (shutdown)
		pthread_cond_wait(&cond, &mutex);
	pthread_mutex_unlock(&mutex);
	::close(fd);
}

void
rf_interface::on(int key)
{
	unsigned char k = key;
	send_packet(PKT_CMD_RCV_ON, 1, &k);
}

void
rf_interface::off()
{
	send_packet(PKT_CMD_RCV_OFF, 0, 0);
}

void
rf_interface::set_auto_dump(FILE *output)
{
	pthread_mutex_lock(&mutex);
	auto_dump = output;
	pthread_mutex_unlock(&mutex);
}

void
rf_interface::set_channel(int channel)
{
	unsigned char c = channel;
	send_packet(PKT_CMD_SET_CHANNEL, 1, &c);
}

void
rf_interface::set_key(int k, const unsigned char *key)
{
	unsigned char x[17];

	x[0] = k;
	::memcpy(&x[1], key, 16);
	send_packet(PKT_CMD_SET_KEY, 17, &x[0]);
}

void
rf_interface::ping()
{
	send_packet(PKT_CMD_PING, 5, (unsigned char *)"test");
}

void
rf_interface::set_mac(const unsigned char *mac)
{
	send_packet(PKT_CMD_SET_MAC, 8, mac);
}

void
rf_interface::send(const unsigned char *mac, const unsigned char *data, int len)
{
	if (len > 128)
		return;
	if (mac) {
		unsigned char x[8+128];
		::memcpy(&x[0], mac, 8);
		::memcpy(&x[8], data, len);
		send_packet(PKT_CMD_SEND_PACKET_MAC, 8+len, &x[0]);
	} else {
		send_packet(PKT_CMD_SEND_PACKET, len, &data[0]);
	}
}

void
rf_interface::send_crypto(int key, const unsigned char *mac, const unsigned char *data, int len)
{
	if (len > 128)
		return;
	if (key >= 8)
		return;
	if (mac) {
		unsigned char x[8+128];
		::memcpy(&x[0], mac, 8);
		::memcpy(&x[8], data, len);
		send_packet(PKT_CMD_SEND_PACKET_CRYPT_MAC+key, 8+len, &x[0]);
	} else {
		send_packet(PKT_CMD_SEND_PACKET_CRYPT+key, len, &data[0]);
	}
}

void
rf_interface::send_packet(int cmd, int len, const unsigned char *data)
{
	unsigned char x[4];
	int sum = 0;

	x[0] = PKT_MAGIC_0;
	x[1] = PKT_MAGIC_1;
	x[2] = cmd;
	x[3] = len;
	sum = (cmd&0xff) + (len&0xff);
	::write(fd, &x[0], 4);
//for (int i = 0; i < 4; i++)printf("%02x ", x[i]);
	if (len) {
		for (int i = 0; i < len; i++)
			sum += data[i];
		::write(fd, data, len);
//for (int i = 0; i < len; i++)printf("%02x ", data[i]);
	}
	x[0] = sum;
	x[1] = sum>>8;
	::write(fd, &x[0], 2);
//for (int i = 0; i < 2; i++)printf("%02x ", x[i]);printf("\n");
}

void 
rf_interface::rf_thread()
	{
	int state = 0;
	int sum2, sum, len, off;
	unsigned char cmd;
	unsigned char b[256];
	unsigned char data[4+8+128];

	pthread_mutex_lock(&mutex);
	for (;;) {
		if (shutdown) {
			pthread_mutex_lock(&mutex);
			shutdown = 0;
			pthread_cond_broadcast(&cond);
			pthread_mutex_unlock(&mutex);
			break;
		}
		int l = read(fd, &b[0], sizeof(b));
		if (l < 0) {
			if (errno != EINTR) {
				pthread_mutex_lock(&mutex);
				while (!shutdown)
					pthread_cond_wait(&cond, &mutex);
				shutdown = 0;
				pthread_cond_broadcast(&cond);
				pthread_mutex_unlock(&mutex);
				break;
			}
			continue;
		}
		for (int i = 0; i < l; i++) {
			switch (state) {
			case 0: if (b[i] == PKT_MAGIC_0)
					state = 1;
				break;
			case 1: if (b[i] == PKT_MAGIC_1)
					state = 2;
				break;
			case 2:	sum = cmd = b[i];
				state = 3;
				break;
			case 3: len = b[i];
				sum += len;
				if (len) {
					if (len >sizeof(data)) {
					    state = 0;
					    break;
					}
					state = 4;
					off = 0;
				} else {
					state = 5;
				}
				break;
			case 4:	data[off++] = b[i];
				sum += b[i];
				if (off >= len)
					state = 5;
				break;
			case 5:	sum2 = b[i];
				state = 6;
				break;
			case 6:	sum2 |= b[i]<<8;
				state = 0;
				sum &= 0xffff;
				if (sum != sum2)
					break;

				switch(cmd) {
				case PKT_CMD_PRINTF:
					data[len-1] = 0;
					if (auto_dump) 
						fprintf(auto_dump, "PRINT: %s", (char *)&data[0]);
					fprintf(stderr, "PRINT: %s", (char *)&data[0]);
					break;
				case PKT_CMD_RCV_PACKET:
					if (callback) 
						(*callback)(this, 0, &data[0], &data[8], len-8);
					goto log;
				case PKT_CMD_RCV_PACKET_CRYPT:
					if (callback) 
						(*callback)(this, 1, &data[0], &data[8], len-8);
log:					if (auto_dump) {
						pthread_mutex_lock(&mutex);
						if (auto_dump) {
							fprintf(auto_dump, "rf %02x%02x%02x%02x:%02x%02x%02x%02x:: ", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);  
							for (int j = 0; j < len; j++) {
								fprintf(auto_dump, "%02x ", data[j]);
								if ((j&3) == 3)
									fprintf(auto_dump, " ");
							}
							fprintf(auto_dump, "\n");
						}
						pthread_mutex_unlock(&mutex);
					}
					break;
				}
				break;
			}
		}
	}
}

void *
rf_interface::thread(void *p)
{
	rf_interface *i = (rf_interface*)p;
	pthread_detach(pthread_self());
	i->rf_thread();
	return 0;
}

