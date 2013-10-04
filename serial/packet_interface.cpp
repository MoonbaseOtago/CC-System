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
#include "suota.h"
#include "protocol.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <zlib.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 
#include <netdb.h>

//
//	C bindings
//
typedef void *rf_handle;
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
rf_on(rf_handle handle)
{
	rf_interface *i = (rf_interface*)handle;
	i->on();
}

void
rf_off(rf_handle handle)
{
	rf_interface *i = (rf_interface*)handle;
	i->off();
}

int
rf_command(rf_handle handle, const char *cmd)
{
	rf_interface *i = (rf_interface*)handle;
	return i->command(cmd);
}

int
rf_initialise(rf_handle handle, const char *file)
{
	rf_interface *i = (rf_interface*)handle;
	return i->initialise(file);
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
rf_set_promiscuous(rf_handle handle, int on)
{
	rf_interface *i = (rf_interface*)handle;
	i->set_promiscuous(on);
}

void
rf_reset(rf_handle handle)
{
	rf_interface *i = (rf_interface*)handle;
	i->reset();
}

void
rf_set_raw(rf_handle handle, int on)
{
	rf_interface *i = (rf_interface*)handle;
	i->set_raw(on);
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

void
rf_send_repeat(rf_handle handle, int secs, int key, unsigned char arch, unsigned char code_base, unsigned long version)
{
	rf_interface *i = (rf_interface*)handle;
	i->send_repeat(secs, key, arch, code_base, version);
}

void
rf_send_repeat_suota_key(rf_handle handle, int secs, unsigned char * key, unsigned char arch, unsigned char code_base, unsigned long version)
{
	rf_interface *i = (rf_interface*)handle;
	i->send_repeat_suota_key(secs, key, arch, code_base, version);
}

int
rf_set_suota_upload(rf_handle handle, unsigned char *key, unsigned char arch, unsigned char code_base, unsigned long version, const char *file)
{
	rf_interface *i = (rf_interface*)handle;
	return i->set_suota_upload(key, arch, code_base, version, file);
}

rf_interface::rf_interface(const char *serial_device, rf_rcv rcv_callback)
{
	struct termios tm;

	suota_list = 0;
	fd = ::open(serial_device, O_RDWR|O_NDELAY);
	if (fd < 0)
		return;
	memset(&current_suota_key[0], 0, sizeof(current_suota_key));
	tcgetattr(fd, &tm);
	cfmakeraw(&tm);
	cfsetspeed(&tm, B9600);
	tcsetattr(fd, TCSAFLUSH, &tm);
	callback = rcv_callback;
	shutdown = 0;
	auto_dump = 0;
	sent = 0;
	dump_outgoing = 0;
	repeat_list = 0;
	pthread_mutex_init(&mutex, 0);
	pthread_mutex_init(&suota_mutex, 0);
	pthread_cond_init(&cond, 0);
	pthread_create(&tid, 0, thread, this);
}

rf_interface::~rf_interface()
{
	suota_upload *sp;

	pthread_mutex_lock(&mutex);
	if (fd >= 0) {
		shutdown = 1;
		::close(fd);
		fd = -1;
		pthread_cond_broadcast(&cond);
		while (shutdown)
			pthread_cond_wait(&cond, &mutex);
	}
	while (repeat_list) {
		suota_repeat *rp = repeat_list;
		rp->quit = 1;
		pthread_cond_broadcast(&rp->cond);
		while (rp->quit)
			pthread_cond_wait(&rp->cond, &mutex);
		repeat_list = rp->next;
		pthread_cond_destroy(&rp->cond);
		free(rp); 
	}
	pthread_mutex_unlock(&mutex);

	while (suota_list) {
		sp = suota_list;
		suota_list = sp->next;
		free(sp->data);
		free(sp);
	}
	pthread_cond_destroy(&cond);
	pthread_mutex_destroy(&mutex);
	pthread_mutex_destroy(&suota_mutex);
}

void
rf_interface::on()
{
	send_packet(PKT_CMD_RCV_ON, 0, 0);
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
rf_interface::send_suota_key(unsigned char *key)
{
	if (memcmp(key, &current_suota_key[0], sizeof(current_suota_key)) == 0)
		return;
	memcpy(&current_suota_key[0], key, sizeof(current_suota_key));
	send_packet(PKT_CMD_SET_SUOTA_KEY, 16, &current_suota_key[0]);
}

void *
rf_interface::repeater(void *p)
{
	suota_repeat *rp = (suota_repeat *)p;
	pthread_detach(pthread_self());

	pthread_mutex_lock(&rp->parent->mutex);
	for (;;) {
		struct timespec timeout;
		struct timeval now;
		packet pkt;

		if (rp->quit) {
			rp->quit = 0;
			pthread_cond_broadcast(&rp->cond);
			pthread_mutex_unlock(&rp->parent->mutex);
			return 0;
		}
		pthread_mutex_unlock(&rp->parent->mutex);
		if (rp->key == suota_key) {
			pthread_mutex_lock(&rp->parent->suota_mutex);
			rp->parent->send_suota_key(&rp->key_value[0]);
		}
		pkt.type = P_TYPE_NOP;
		pkt.arch = rp->arch;
		pkt.code_base = rp->code_base;
		pkt.version[0] = rp->version;
		pkt.version[1] = rp->version>>8;
		rp->parent->send_crypto(rp->key, 0, (unsigned char *)&pkt, sizeof(pkt));
		if (rp->key == suota_key) 
			pthread_mutex_unlock(&rp->parent->suota_mutex);
		pthread_mutex_lock(&rp->parent->mutex);
		gettimeofday(&now, 0);
		timeout.tv_sec = now.tv_sec + rp->secs;
              	timeout.tv_nsec = now.tv_usec * 1000;
		while (!rp->quit &&  pthread_cond_timedwait(&rp->cond, &rp->parent->mutex, &timeout) != ETIMEDOUT)
			;
	}
}

void
rf_interface::send_repeat(int secs, int key, unsigned char arch, unsigned char code_base, unsigned long version)
{
	send_repeat(secs, key, 0, arch, code_base, version);
}

void
rf_interface::send_repeat_suota_key(int secs, unsigned char *key, unsigned char arch, unsigned char code_base, unsigned long version)
{
	send_repeat(secs, suota_key, key, arch, code_base, version);
}


void
rf_interface::send_repeat(int secs, int key, unsigned char * key_value, unsigned char arch, unsigned char code_base, unsigned long version)
{
	suota_repeat *rp;
	pthread_t t;

	if (secs <= 0) {
		suota_repeat *last = 0;

		pthread_mutex_lock(&mutex);
		for (rp = repeat_list; rp; rp=rp->next) {
			if (rp->key == key && rp->arch == arch && rp->code_base == code_base && rp->version == version) {
				rp->quit = 1;
				pthread_cond_broadcast(&rp->cond);
				while (rp->quit)
					pthread_cond_wait(&rp->cond, &mutex);
				if (last) {
					last->next = rp->next;
				} else {
					repeat_list = rp->next;
				}
				pthread_cond_destroy(&rp->cond);
				free(rp);
				break;
			}
			last = rp;
		}
		pthread_mutex_unlock(&mutex);
		return;
	}
	rp = (suota_repeat *)malloc(sizeof(*rp));
	if (!rp)
		return;
	pthread_mutex_lock(&mutex);
	rp->parent = this;
	rp->secs = secs;
	rp->next = repeat_list;
	repeat_list = rp;
	rp->key = key;
	if (key_value)
		memcpy(&rp->key_value[0], key_value, sizeof(rp->key_value));
	rp->arch = arch;
	rp->code_base = code_base;
	rp->version = version;
	rp->quit = 0;
	pthread_cond_init(&rp->cond, 0);
	pthread_create(&t, 0, repeater, rp);
	pthread_mutex_unlock(&mutex);
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
rf_interface::set_raw(int on)
{
	unsigned char c = (on?1:0);
	send_packet(PKT_CMD_SET_RAW, 1, &c);
}

void
rf_interface::set_suota_enable(int on)
{
	unsigned char c = (on?1:0);
	send_packet(PKT_CMD_SET_SUOTA, 1, &c);
}

void
rf_interface::reset(void)
{
	memset(&current_suota_key[0], 0, sizeof(current_suota_key));
	send_packet(PKT_CMD_RESET, 0, 0);
}

void
rf_interface::set_promiscuous(int on)
{
	unsigned char c = (on?1:0);
	send_packet(PKT_CMD_SET_PROMISCUOUS, 1, &c);
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
	unsigned char cmd;

	if (len > 128)
		return;
	if (key >= 8 && key != suota_key)
		return;
	if (key == suota_key) {
		if (mac) {
			cmd = PKT_CMD_SEND_PACKET_CRYPT_SUOTA_MAC;
		} else {
			cmd = PKT_CMD_SEND_PACKET_CRYPT_SUOTA;
		}
	} else {
		if (mac) {
			cmd = PKT_CMD_SEND_PACKET_CRYPT_MAC+key;
		} else {
			cmd = PKT_CMD_SEND_PACKET_CRYPT+key;
		}
	}
	if (mac) {
		unsigned char x[8+128];
		::memcpy(&x[0], mac, 8);
		::memcpy(&x[8], data, len);
		send_packet(cmd, 8+len, &x[0]);
	} else {
		send_packet(cmd, len, &data[0]);
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
	if (dump_outgoing)
		for (int i = 0; i < 4; i++)printf("%02x ", x[i]);
	if (len) {
		for (int i = 0; i < len; i++)
			sum += data[i];
		::write(fd, data, len);
		if (dump_outgoing)
			for (int i = 0; i < len; i++)printf("%02x ", data[i]);
	}
	x[0] = sum;
	x[1] = sum>>8;
	::write(fd, &x[0], 2);
	if (dump_outgoing) {
		for (int i = 0; i < 2; i++)printf("%02x ", x[i]);
		printf("\n");
	}
}

bool 
rf_interface::data_receive(bool crypto, unsigned char key, unsigned char *mac, unsigned char *p, int len)
{
	return 0;
}

void 
rf_interface::rf_thread()
	{
	int state = 0;
	int sum2, sum, len, off;
	unsigned char cmd;
	unsigned char b[256];
	unsigned char data[4+8+128+256];

	pthread_mutex_lock(&mutex);
	for (;;) {
		if (shutdown) {
			shutdown = 0;
			pthread_cond_broadcast(&cond);
			pthread_mutex_unlock(&mutex);
			break;
		}
		pthread_mutex_unlock(&mutex);
		struct pollfd fds = {fd, POLLIN|POLLPRI|POLLRDHUP|POLLHUP|POLLNVAL, 0};
		int l = poll(&fds, 1, 5);
		if (l == 0) {
			pthread_mutex_lock(&mutex);
			continue;
		}
		if (l > 0) 
			l = ::read(fd, &b[0], sizeof(b));
		pthread_mutex_lock(&mutex);
		if (l < 0) {
			if (errno != EINTR && errno != EAGAIN) {
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
//fprintf(stderr, "b[%d/%d]: %02x\n", i, l, b[i]);
			switch (state) {
			case 0: if (b[i] == PKT_MAGIC_0) {
					state = 1;
				} else {
					fprintf(stderr, "U: %02x '%c'\n", b[i], b[i]);
				}
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
					if (auto_dump != stderr)
						fprintf(stderr, "PRINT: %s", (char *)&data[0]);
					break;
				case PKT_CMD_RCV_PACKET:
				case PKT_CMD_RCV_PACKET_BROADCAST:
					if (data_receive(0, 0, &data[0], &data[8], len-8))
						break;
					if (callback) 
						(*callback)(this, cmd == PKT_CMD_RCV_PACKET_BROADCAST?1:0, 0, 0, &data[0], &data[8], len-8);
					goto log;
				case PKT_CMD_RCV_PACKET_CRYPT:
				case PKT_CMD_RCV_PACKET_CRYPT_BROADCAST:
					if (data_receive(1, data[0], &data[1], &data[9], len-9))
						break;
					if (suota_list && ((packet *)&data[9])->type == P_TYPE_SUOTA_REQ) {
						packet *p = (packet *)&data[9];
						suota_req *srp = (suota_req*)&p->data[0];
						unsigned char key = data[0];
						for (suota_upload *sp = suota_list; sp; sp=sp->next) 
						if (srp->arch == sp->arch &&
						    srp->code_base == sp->code_base &&
						    srp->version[0] == (sp->version&0xff) &&
						    srp->version[1] == ((sp->version>>8)&0xff)) {
							unsigned char b[100];
							int xlen;
							packet *pp = (packet *)&b[0];
							suota_resp *rp = (suota_resp *)&pp->data[0];
							unsigned int offset = srp->offset[0]|(srp->offset[1]<<8);
							pp->type = P_TYPE_SUOTA_RESP;
							pp->arch = sp->arch;
							pp->code_base = sp->code_base;
							pp->version[0] = sp->version;
							pp->version[1] = sp->version>>8;
							rp->arch = sp->arch;
							rp->code_base = sp->code_base;
							rp->version[0] = sp->version;
							rp->version[1] = sp->version>>8;
							rp->offset[0] = offset;
							rp->offset[1] = offset>>8;
							rp->total_len[0] = sp->len;
							rp->total_len[1] = sp->len>>8;
							if (offset < sp->len) {
								xlen = sp->len - offset;
								if (xlen > 64)
								    xlen = 64;
								memcpy(&rp->data[0], &sp->data[offset], xlen);
							} else {
								xlen = 0;
							}
							if ((offset+xlen) >= sp->len)
								sent = 1;
							//fprintf(stderr, "sending SUOTA resp offset = 0x%x len=%d\n", offset, xlen);
							xlen += 8;	// resp header
							xlen += 5;	// packet header
							pthread_mutex_lock(&suota_mutex);
							send_suota_key(&sp->key[0]);
							send_crypto(suota_key, &data[1], (unsigned char *)pp, xlen);
							pthread_mutex_unlock(&suota_mutex);
			
							goto log;
						}
						
					}
					if (callback) 
						(*callback)(this, cmd == PKT_CMD_RCV_PACKET_CRYPT_BROADCAST?1:0, 1, data[0], &data[1], &data[9], len-9);
log:					if (auto_dump) {
						int o = cmd == PKT_CMD_RCV_PACKET_CRYPT_BROADCAST||cmd == PKT_CMD_RCV_PACKET_CRYPT?1:0;
						int k = (o?data[0]:0);
						fprintf(auto_dump, "rf %c %d %3d %02x%02x%02x%02x:%02x%02x%02x%02x:: ", cmd == PKT_CMD_RCV_PACKET_CRYPT_BROADCAST||cmd == PKT_CMD_RCV_PACKET_BROADCAST?'B':' ', k, len-8, data[o+0], data[o+1], data[o+2], data[o+3], data[o+4], data[o+5], data[o+6], data[o+7]);  
						for (int j = 8+o; j < len; j++) {
							fprintf(auto_dump, "%02x ", data[j]);
							if ((j&3) == 3)
								fprintf(auto_dump, " ");
						}
						fprintf(auto_dump, "\n");
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

//
//	scripted startup
//
//
//	commands:
//
//	k N hexstring - set key N to the string
//	K N	      - set the current key to N (do this last)
//	O	      - on - equivalent to K 0 
//	o	      - off
//	c N	      - channel N
//	u K A V file  - offer file for suota requests for version V of architecture A encrypted with key K
//	s A V	      - advertise version V of architecture A 
//	m MAC         - set the mac address to the hex string
//	a	      - turn on auto dump to stderr
//	#	      -	comment

int
rf_interface::initialise(const char *file)
{
	int res = 1, line;

	if (!opened_ok())
		return 0;
	FILE *f = fopen(file, "r");
	if (!f) {
		fprintf(stderr, "rf_interface::initialise(): can't open file '%s'\n", file);
		return 0;
	}
	line = 0;
	for (;;) {
		char b[1024];

		if (!fgets(b, sizeof(b), f)) {
			break;
		}
		line++;
		char *cp = b;
		while (*cp == ' ' || *cp == '\t')
			cp++;
		int i = strlen(cp);
		if (i > 0 && cp[i-1] == '\n')
			cp[i-1] = 0;
		char c = *cp;
		if (!c || c == '#')
			continue;
		res = command(cp, file, line);
		if (!res) 
			break;
	}
	fclose(f);
	return res;
}

static const char *
hdr(const char *file, int line, char *tmp, int size)
{
	if (!file)
		return "error";
	snprintf(tmp, size, "rf_interface::initialise(%s) line %d", file, line);
	return &tmp[0];
}

int
rf_interface::command(const char *cc, const char *file, int line)
{
	char *cp = (char *)cc;	// cast to make strol happy
	int secs, arch, code_base, version, k;
	unsigned char key[16];
	unsigned char sk[16];
	char tmp[100];
	char c = *cp++;
	int res = 1;
	int i;
	unsigned char *macp;
	unsigned char data[128];
	int len;

	while (*cp == ' ' || *cp == '\t')
		cp++;
	switch (c) {
	case 0:
		return res;
	case 'o':	
		off();
		break;
	case 'O':
		on();
		break;
	case 'p':
		ping();
		break;
	case 'P':
		set_promiscuous(*cp == '-'?0:1);
		break;
	case 'R':
		reset();
		break;
	case 'r':
		set_raw(*cp == '-'?0:1);
		break;
	case 'a':
		set_auto_dump(*cp == '-'?0:stderr);
		break;
	case 'E':
		set_suota_enable(*cp == '-'?0:1);
		break;
	case 'D':
		dump_outgoing = *cp == '-'?0:1;
		break;
	case 'A':
		set_auto_dump(*cp == '-'?0:stderr);
		set_promiscuous(*cp == '-'?0:1);
		set_raw(*cp == '-'?0:1);
		break;
	case 'c':
		i = strtol(cp, 0, 0);
		if (i < 11 || i > 26) {
			fprintf(stderr, "%s: invalid channel number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), i);
			res = 0;
		} else {
			set_channel(i);
		}
		break;
	case 'K':
		k = strtol(cp, &cp, 0);
		if (k < 0 || k >= 8) {
                               fprintf(stderr, "%s: invalid key number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), k);
                               res = 0;
			break;
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		memset(key, 0, sizeof(key));
		i = 0;
		for (;;) {
			if (i == 16 || !*cp || *cp == ' ' || *cp == '\n') {
				if (i == 0) {
					res = 0;
                               		fprintf(stderr, "%s: no key found\n", hdr(file, line, &tmp[0], sizeof(tmp)));
					break;
				}
				set_key(k, key);
				break;
			}
			if (i != 0 && *cp == ':')
				cp++;
			c = *cp++;
			if (c >= '0' && c <= '9') {
				key[i] = (c-'0')<<4;
			} else
			if (c >= 'a' && c <= 'f') {
				key[i] = (c-'a'+10)<<4;
			} else
			if (c >= 'A' && c <= 'F') {
				key[i] = (c-'A'+10)<<4;
			} else {
				res = 0;
                               	fprintf(stderr, "%s: bad character found in hex key '%c'\n", hdr(file, line, &tmp[0], sizeof(tmp)), c?c:'?');
				break;
			}
			c = *cp++;
			if (c >= '0' && c <= '9') {
				key[i] |= (c-'0');
			} else
			if (c >= 'a' && c <= 'f') {
				key[i] |= (c-'a'+10);
			} else
			if (c >= 'A' && c <= 'F') {
				key[i] |= (c-'A'+10);
			} else {
				res = 0;
                               	fprintf(stderr, "%s: bad character found in hex key '%c'\n", hdr(file, line, &tmp[0], sizeof(tmp)), c?c:'?');
				break;
			}
			i++;
		}
		break;
	case 'm':
		memset(key, 0, sizeof(key));
		i = 0;
		for (;;) {
			if (i == 8 || !*cp || *cp == ' ' || *cp == '\n') {
				if (i == 0) {
					res = 0;
                               		fprintf(stderr, "%s: no mac found\n", hdr(file, line, &tmp[0], sizeof(tmp)));
					break;
				}
				set_mac(key);
				break;
			}
			if (i != 0 && *cp == ':')
				cp++;
			c = *cp++;
			if (c >= '0' && c <= '9') {
				key[i] = (c-'0')<<4;
			} else
			if (c >= 'a' && c <= 'f') {
				key[i] = (c-'a'+10)<<4;
			} else
			if (c >= 'A' && c <= 'F') {
				key[i] = (c-'A'+10)<<4;
			} else {
				res = 0;
                               	fprintf(stderr, "%s: bad character found in hex mac address '%c'\n", hdr(file, line, &tmp[0], sizeof(tmp)), c?c:'?');
				break;
			}
			c = *cp++;
			if (c >= '0' && c <= '9') {
				key[i] |= (c-'0');
			} else
			if (c >= 'a' && c <= 'f') {
				key[i] |= (c-'a'+10);
			} else
			if (c >= 'A' && c <= 'F') {
				key[i] |= (c-'A'+10);
			} else {
				res = 0;
                               	fprintf(stderr, "%s: bad character found in hex mac address '%c'\n", hdr(file, line, &tmp[0], sizeof(tmp)), c?c:'?');
				break;
			}
			i++;
		}
		break;
	case '!':	// ! key *  len [data ...]	// crypto
			// ! key mac len [data ...]	// crypto
		while (*cp == ' ' || *cp == '\t')
			cp++;
		if (!*cp) {
			res = 0;
                        fprintf(stderr, "%s: no key number found\n", hdr(file, line, &tmp[0], sizeof(tmp)));
			break;
		}
		k = strtoul(cp, &cp, 0);
	case 's':	// s *  len [data ...]
			// s mac len [data ...]
		while (*cp == ' ' || *cp == '\t')
			cp++;
		if (*cp == '*') {
			cp++;
			macp = 0;
		} else {
			memset(key, 0, sizeof(key));
			i = 0;
			for (;;) {
				if (i == 8 || !*cp || *cp == ' ' || *cp == '\n') {
					if (i == 0) {
						res = 0;
                               			fprintf(stderr, "%s: no mac found\n", hdr(file, line, &tmp[0], sizeof(tmp)));
						break;
					}
					macp = &key[0];
					break;
				}
				if (i != 0 && *cp == ':')
					cp++;
				c = *cp++;
				if (c >= '0' && c <= '9') {
					key[i] = (c-'0')<<4;
				} else
				if (c >= 'a' && c <= 'f') {
					key[i] = (c-'a'+10)<<4;
				} else
				if (c >= 'A' && c <= 'F') {
					key[i] = (c-'A'+10)<<4;
				} else {
					res = 0;
                               		fprintf(stderr, "%s: bad character found in hex mac address '%c'\n", hdr(file, line, &tmp[0], sizeof(tmp)), c?c:'?');
					break;
				}
				c = *cp++;
				if (c >= '0' && c <= '9') {
					key[i] |= (c-'0');
				} else
				if (c >= 'a' && c <= 'f') {
					key[i] |= (c-'a'+10);
				} else
				if (c >= 'A' && c <= 'F') {
					key[i] |= (c-'A'+10);
				} else {
					res = 0;
                               		fprintf(stderr, "%s: bad character found in hex mac address '%c'\n", hdr(file, line, &tmp[0], sizeof(tmp)), c?c:'?');
					break;
				}
				i++;
			}
			if (!res)
				break;
		}
		while (*cp == ' ' || *cp == '\t')
			cp++;
		if (*cp < '0' || *cp > '9') {
			fprintf(stderr, "%s: packet length expected '%s'\n", hdr(file, line, &tmp[0], sizeof(tmp)), cp);
			res = 0;
			break;
		}
		len = strtoul(cp, &cp, 0);
		if (len > 100)
			len = 100;
		for (i = 0; i < len; i++) {
			int v;
			while (*cp == ' ' || *cp == '\t')
				cp++;
			char x = *cp;
			if (x >= '0' && x <= '9') {
				v = x-'0';
			} else
			if (x >= 'a' && x <= 'f') {
				v = x-'a'+10;
			} else
			if (x >= 'A' && x <= 'F') {
				v = x-'A'+10;
			} else {
				fprintf(stderr, "%s: invalid packet data '%s'\n", hdr(file, line, &tmp[0], sizeof(tmp)), cp);
				res = 0;
				break;
			}
			cp++;
			x = *cp++;
			if (x >= '0' && x <= '9') {
				v = (v<<4)+(x-'0');
			} else
			if (x >= 'a' && x <= 'f') {
				v = (v<<4)+(x-'a'+10);
			} else
			if (x >= 'A' && x <= 'F') {
				v = (v<<4)+(x-'A'+10);
			} else {
				cp--;
			}
			data[i] = v;
		}
		if (!res)
			break;
		for (;i < len; i++)
			data[i] = rand();
		if (c == '!') {
			send_crypto(0, macp, data, len);
		} else {
			send(macp, data, len);
		}
		break;
	case 'U':
		secs = strtol(cp, &cp, 0);
		if (secs < 0) {
                        fprintf(stderr, "%s: invalid seconds number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), secs);
                        res = 0;
			break;
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		if (*cp >= '0' && *cp <= '7' && (cp[1] == ' ' || cp[1] == '\t' || !cp[1] || cp[1]=='\n')) {
			k = strtol(cp, &cp, 0);
			if (k < 0 || k >= 8) {
                        	fprintf(stderr, "%s: invalid key number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), k);
                        	res = 0;
				break;
			}
		} else {
			k = suota_key;
			memset(sk, 0, sizeof(sk));
			i = 0;
			for (;;) {
				if (i == 16 || !*cp || *cp == ' ' || *cp == '\n') {
					if (i == 0) {
						res = 0;
                               			fprintf(stderr, "%s: no key found\n", hdr(file, line, &tmp[0], sizeof(tmp)));
						break;
					}
					break;
				}
				if (i != 0 && *cp == ':')
					cp++;
				c = *cp++;
				if (c >= '0' && c <= '9') {
					sk[i] = (c-'0')<<4;
				} else
				if (c >= 'a' && c <= 'f') {
					sk[i] = (c-'a'+10)<<4;
				} else
				if (c >= 'A' && c <= 'F') {
					sk[i] = (c-'A'+10)<<4;
				} else {
					res = 0;
                               		fprintf(stderr, "%s: bad character found in hex key '%c'\n", hdr(file, line, &tmp[0], sizeof(tmp)), c?c:'?');
					break;
				}
				c = *cp++;
				if (c >= '0' && c <= '9') {
					sk[i] |= (c-'0');
				} else
				if (c >= 'a' && c <= 'f') {
					sk[i] |= (c-'a'+10);
				} else
				if (c >= 'A' && c <= 'F') {
					sk[i] |= (c-'A'+10);
				} else {
					res = 0;
                               		fprintf(stderr, "%s: bad character found in hex key '%c'\n", hdr(file, line, &tmp[0], sizeof(tmp)), c?c:'?');
					break;
				}
				i++;
			}
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		if (!*cp) {
			send_suota_key(&sk[0]);
			break;
		}
		arch = strtol(cp, &cp, 0);
		if (arch < 0 || arch >= 256) {
                        fprintf(stderr, "%s: invalid architecture number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), arch);
                        res = 0;
			break;
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		code_base = strtol(cp, &cp, 0);
		if (code_base < 0 || code_base >= 256) {
                        fprintf(stderr, "%s: invalid code_base number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), code_base);
                        res = 0;
			break;
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		version = strtol(cp, &cp, 0);
		if (version < 0 || version >= (1<<16)) {
                        fprintf(stderr, "%s: invalid architecture version %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), version);
                        res = 0;
			break;
		}
		send_repeat(secs, k, &sk[0], arch, code_base, version);
		break;
	case 'u':
		k = 1;
		memset(sk, 0, sizeof(sk));
		i = 0;
		if (*cp == '*') {
			k = 0;
		} else
		for (;;) {
			if (i == 16 || !*cp || *cp == ' ' || *cp == '\n') {
				if (i == 0) {
					res = 0;
                               		fprintf(stderr, "%s: no key found\n", hdr(file, line, &tmp[0], sizeof(tmp)));
					break;
				}
				break;
			}
			if (i != 0 && *cp == ':')
				cp++;
			c = *cp++;
			if (c >= '0' && c <= '9') {
				sk[i] = (c-'0')<<4;
			} else
			if (c >= 'a' && c <= 'f') {
				sk[i] = (c-'a'+10)<<4;
			} else
			if (c >= 'A' && c <= 'F') {
				sk[i] = (c-'A'+10)<<4;
			} else {
				res = 0;
                               	fprintf(stderr, "%s: bad character found in hex key '%c'\n", hdr(file, line, &tmp[0], sizeof(tmp)), c?c:'?');
				break;
			}
			c = *cp++;
			if (c >= '0' && c <= '9') {
				sk[i] |= (c-'0');
			} else
			if (c >= 'a' && c <= 'f') {
				sk[i] |= (c-'a'+10);
			} else
			if (c >= 'A' && c <= 'F') {
				sk[i] |= (c-'A'+10);
			} else {
				res = 0;
                               	fprintf(stderr, "%s: bad character found in hex key '%c'\n", hdr(file, line, &tmp[0], sizeof(tmp)), c?c:'?');
				break;
			}
			i++;
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		arch = strtol(cp, &cp, 0);
		if (arch < 0 || arch >= 256) {
                        fprintf(stderr, "%s: invalid architecture number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), arch);
                        res = 0;
			break;
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		code_base = strtol(cp, &cp, 0);
		if (code_base < 0 || code_base >= 256) {
                        fprintf(stderr, "%s: invalid code_base number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), code_base);
                        res = 0;
			break;
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		version = strtol(cp, &cp, 0);
		if (version < 0 || version >= (1<<16)) {
                        fprintf(stderr, "%s: invalid architecture version %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), version);
                        res = 0;
			break;
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		i = strlen(cp);
		while (i > 0 && cp[i-1] == ' ' || cp[i-1] == '\t') {
			i--;
			cp[i] = 0;
		}
		if (!*cp) {
                        fprintf(stderr, "%s: file name expected\n", hdr(file, line, &tmp[0], sizeof(tmp)));
                        res = 0;
			break;
		}
		if (!set_suota_upload((k?&sk[0]:0), arch, code_base, version, cp)) {
                        fprintf(stderr, "%s: loading file '%s' failed\n", hdr(file, line, &tmp[0], sizeof(tmp)), cp);
                        res = 0;
		}
		break;
	default:
		if (!virtual_command(cc, file, line)) {
			fprintf(stderr, "%s: unknown command '%s'\n", hdr(file, line, &tmp[0], sizeof(tmp)), cp-1);
			res = 0;
		}
		break;
	}
	return res;
}

bool
rf_interface::virtual_command(const char *cc, const char *file, int line) 
{
	return 0;
}

static int
hex(FILE *f)
{
        char c1, c2;
        int r;
	c1 = fgetc(f);
        if (c1 < 0)
                return -1;
	c2 = fgetc(f);
        if (c2 < 0)
                return -1;
        if (c1 >= '0' && c1 <= '9') {
                r = c1-'0';
        } else
        if (c1 >= 'A' && c1 <= 'F') {
                r = c1-'A'+10;
        } else
        if (c1 >= 'a' && c1 <= 'f') {
                r = c1-'a'+10;
        } else return -1;
        r=r<<4;
        if (c2 >= '0' && c2 <= '9') {
                r += c2-'0';
        } else
        if (c2 >= 'A' && c2 <= 'F') {
                r += c2-'A'+10;
        } else
        if (c2 >= 'a' && c2 <= 'f') {
                r += c2-'a'+10;
        } else return -1;
        return r;
}


int
rf_interface::set_suota_upload(unsigned char *key, unsigned char arch, unsigned char code_base, unsigned long version, const char *file, load_info *out)
{
	FILE *f = fopen(file, "r");
	unsigned char hdr[4];
	unsigned char k[16];
	if (!f)
		return 0;
	
	if (fread(&hdr[0], 1, 4, f) != 4) {
		fclose(f);
		return 0;
	}
	if (fread(&k[0], 1, 16, f) != 16) {
		fclose(f);
		return 0;
	}
	unsigned short addr = hdr[0]|(hdr[1]<<8);
	unsigned short len = hdr[2]|(hdr[3]<<8);
	unsigned char *data = (unsigned char *)malloc(len);
	if (!data) {
		fclose(f);
		return 0;
	}
	if (fread(data, 1, len, f) != len) {
		free(data);
		fclose(f);
		return 0;
	}
	fclose(f);
	if (!key) {
		arch = data[6];
	} else
	if (arch != data[6]) {
		fprintf(stderr, "file '%s' architecture tag (%d) does not match request (%d)\n", file, data[6], arch);
		free(data);
		return 0;
	}
	if (!key) {
		code_base = data[7];
	} else
	if (code_base != data[7]) {
		fprintf(stderr, "file '%s' code_base tag (%d) does not match request (%d)\n", file, data[7], code_base);
		free(data);
		return 0;
	}
	int t = data[8]|(data[9]<<8);
	if (!key) {
		version = t;
	} else
	if (version != t) {
		fprintf(stderr, "file '%s' version tag (%d) does not match request (%ld)\n", file, t, version);
		free(data);
		return 0;
	}
	if (out) {
		memcpy(&out->key[0], &k[0], sizeof(out->key));
		out->arch = arch;
		out->code_base = code_base;
		out->version = version;
	}
	suota_upload *sp = (suota_upload *)malloc(sizeof(*sp));
	if (!sp) {
		free(data);
		return 0;
	}
	sp->len = len;
	sp->data = data;
	if (key) {
		memcpy(&sp->key[0], key, sizeof(sp->key));
	} else {
		memcpy(&sp->key[0], &k[0], sizeof(sp->key));
	}
	sp->arch = arch;
	sp->base = addr;
	sp->code_base = code_base;
	sp->version = version;
	sp->next = suota_list;
	suota_list = sp;
	return  1;
} 

