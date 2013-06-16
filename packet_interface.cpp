//
// Copyright 2013 Paul Campbell paul@taniwha.com
//
#include "packet_interface.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

typedef void *rf_handle;
typedef void (*rf_rcv)(rf_handle, int crypt, unsigned char *mac, unsigned char *data, int len);
rf_handle
rf_open(char *serial_device, rf_rcv rcv_callback)
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
rf_set_key(rf_handle handle, int k, unsigned char *key)
{
	rf_interface *i = (rf_interface*)handle;
	i->set_key(k, key);
}

void
rf_set_mac(rf_handle handle, unsigned char *mac)
{
	rf_interface *i = (rf_interface*)handle;
	i->set_mac(mac);
}

void
rf_send(rf_handle handle, unsigned char *mac, unsigned char *data, int len)
{
	rf_interface *i = (rf_interface*)handle;
	i->send(mac, data, len);
}

void
rf_send_crypto(rf_handle handle, int key, unsigned char *mac, unsigned char *data, int len)
{
	rf_interface *i = (rf_interface*)handle;
	i->send_crypto(key, mac, data, len);
}


rf_interface::rf_interface(char *serial_device, rf_rcv rcv_callback)
{
	fd = ::open(serial_device, O_RDWR);
	if (fd < 0)
		return;
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
rf_interface::set_key(int k, unsigned char *key)
{
	unsigned char x[17];

	x[0] = k;
	::memcpy(&x[1], key, 16);
	send_packet(PKT_CMD_SET_KEY, 17, &x[0]);
}

void
rf_interface::set_mac(unsigned char *mac)
{
	send_packet(PKT_CMD_SET_MAC, 8, mac);
}

void
rf_interface::send(unsigned char *mac, unsigned char *data, int len)
{
	unsigned char x[8+128];
	if (len > 128)
		return;
	::memcpy(&x[0], mac, 8);
	::memcpy(&x[8], data, len);
	send_packet(PKT_CMD_SEND_PACKET, 8+len, &x[0]);
}

void
rf_interface::send_crypto(int key, unsigned char *mac, unsigned char *data, int len)
{
	unsigned char x[1+8+128];
	if (len > 128)
		return;
	if (key >= 8)
		return;
	x[0] = key;
	::memcpy(&x[1], mac, 8);
	::memcpy(&x[9], data, len);
	send_packet(PKT_CMD_SEND_PACKET_CRYPT, 1+8+len, &x[0]);
}

void
rf_interface::send_packet(int cmd, int len, unsigned char *data)
{
	unsigned char x[4];
	int sum = 0;

	x[0] = PKT_MAGIC_0;
	x[1] = PKT_MAGIC_1;
	x[2] = cmd;
	x[3] = len;
	sum = (cmd&0xff) + (len&0xff);
	::write(fd, &x[0], 4);
	if (len) {
		for (int i = 0; i < len; i++)
			sum += data[i];
		::write(fd, data, len);
	}
	x[0] = sum>>8;
	x[1] = sum;
	::write(fd, &x[0], 2);
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
			case 5:	sum2 = b[i]<<8;
				state = 6;
				break;
			case 6:	sum2 |= b[i];
				state = 0;
				sum &= 0xffff;
				if (sum != sum2)
					break;

				switch(sum) {
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

