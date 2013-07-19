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


//
//	C bindings
//
typedef void *rf_handle;
typedef void (*rf_rcv)(rf_handle, int broadcast, int crypt, unsigned char *mac, unsigned char *data, int len);
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
rf_set_suota_version(rf_handle handle, unsigned char arch, unsigned long version)
{
	rf_interface *i = (rf_interface*)handle;
	i->set_suota_version(arch, version);
}

int
rf_set_suota_upload(rf_handle handle, int key, unsigned char arch, unsigned long version, const char *file)
{
	rf_interface *i = (rf_interface*)handle;
	return i->set_suota_upload(key, arch, version, file);
}

rf_interface::rf_interface(const char *serial_device, rf_rcv rcv_callback)
{
	struct termios tm;

	suota_list = 0;
	fd = ::open(serial_device, O_RDWR|O_NDELAY);
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
	suota_upload *sp;
	if (fd >= 0) {
		pthread_mutex_lock(&mutex);
		shutdown = 1;
		::close(fd);
		fd = -1;
		pthread_cond_broadcast(&cond);
		while (shutdown)
			pthread_cond_wait(&cond, &mutex);
		pthread_mutex_unlock(&mutex);
	}

	while (suota_list) {
		sp = suota_list;
		suota_list = sp->next;
		free(sp->data);
		free(sp);
	}
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
rf_interface::set_suota_version(unsigned char arch, unsigned long version)
{
	unsigned char x[4];
	x[0] = arch;
	x[1] = version;
	x[2] = version>>8;
	x[3] = version>>16;
	send_packet(PKT_CMD_SET_SUOTA_VERSION, 4, &x[0]);
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
	rf_id[0] = mac[6];
	rf_id[1] = mac[7];
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
for (int i = 0; i < 4; i++)printf("%02x ", x[i]);
	if (len) {
		for (int i = 0; i < len; i++)
			sum += data[i];
		::write(fd, data, len);
for (int i = 0; i < len; i++)printf("%02x ", data[i]);
	}
	x[0] = sum;
	x[1] = sum>>8;
	::write(fd, &x[0], 2);
for (int i = 0; i < 2; i++)printf("%02x ", x[i]);printf("\n");
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
					fprintf(stderr, "PRINT: %s", (char *)&data[0]);
					break;
				case PKT_CMD_RCV_PACKET:
				case PKT_CMD_RCV_PACKET_BROADCAST:
					if (callback) 
						(*callback)(this, cmd == PKT_CMD_RCV_PACKET_BROADCAST?1:0, 0, &data[0], &data[8], len-8);
					goto log;
				case PKT_CMD_RCV_PACKET_CRYPT:
				case PKT_CMD_RCV_PACKET_CRYPT_BROADCAST:
					if (suota_list && ((packet *)&data[8])->type == P_TYPE_SUOTA_REQ) {
						packet *p = (packet *)&data[8];
						suota_req *srp = (suota_req*)&p->data[0];
						if (srp->id[0] != rf_id[0] || srp->id[1] != rf_id[1])
							goto log;
						for (suota_upload *sp = suota_list; sp; sp=sp->next) 
						if (srp->arch == sp->arch &&
						    srp->version[0] == (sp->version&0xff) &&
						    srp->version[1] == ((sp->version>>8)&0xff) &&
						    srp->version[2] == ((sp->version>>16)&0xff)) {	
							unsigned char b[100];
							packet *pp = (packet *)&b[0];
							suota_resp *rp = (suota_resp *)&pp->data[0];
							unsigned int offset = srp->offset[0]|(srp->offset[1]<<8);
							pp->type = P_TYPE_SUOTA_RESP;
							rp->arch = sp->arch;
							rp->version[0] = sp->version;
							rp->version[1] = sp->version>>8;
							rp->version[2] = sp->version>>16;
							rp->offset[0] = offset;
							rp->offset[1] = offset>>8;
							rp->total_len[0] = sp->len;
							rp->total_len[1] = sp->len>>8;
							if (offset < sp->len) {
								len = sp->len - offset;
								if (len > 64)
								    len = 64;
								memcpy(&rp->data[0], &sp->data[offset], len);
							} else {
								len = 0;
							}
							len += 8;	// resp header
							len += 10;	// packet header
							send_crypto(sp->key, &data[0], (unsigned char *)pp, len);
							goto log;
						}
						
					}
					if (callback) 
						(*callback)(this, cmd == PKT_CMD_RCV_PACKET_CRYPT_BROADCAST?1:0, 1, &data[0], &data[8], len-8);
log:					if (auto_dump) {
						if (auto_dump) {
							fprintf(auto_dump, "rf %c %02x%02x%02x%02x:%02x%02x%02x%02x:: ", cmd == PKT_CMD_RCV_PACKET_CRYPT_BROADCAST||cmd == PKT_CMD_RCV_PACKET_BROADCAST?'B':' ', data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);  
							for (int j = 0; j < len; j++) {
								fprintf(auto_dump, "%02x ", data[j]);
								if ((j&3) == 3)
									fprintf(auto_dump, " ");
							}
							fprintf(auto_dump, "\n");
						}
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
	if (!f)
		return 0;
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
	int arch, version, k;
	unsigned char key[16];
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
		on(0);
		break;
	case 'p':
		ping();
		break;
	case 'a':
		set_auto_dump(*cp == '-'?0:stderr);
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
	case 'k':
		k = strtol(cp, 0, 0);
		if (k < 0 || k >= 8) {
			fprintf(stderr, "%s: invalid key number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), k);
			res = 0;
		} else {
			on(i);
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
	case 'S':
		arch = strtol(cp, &cp, 0);
		if (arch < 0 || arch >= 256) {
                        fprintf(stderr, "%s: invalid architecture number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), arch);
                        res = 0;
			break;
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		version = strtol(cp, &cp, 0);
		if (version < 0 || version >= (1<<24)) {
                        fprintf(stderr, "%s: invalid architecture version %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), arch);
                        res = 0;
			break;
		}
		set_suota_version(arch, version);
		break;
	case 'u':
		arch = strtol(cp, &cp, 0);
		if (arch < 0 || arch >= 256) {
                        fprintf(stderr, "%s: invalid architecture number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), arch);
                        res = 0;
			break;
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		k = strtol(cp, &cp, 0);
		if (k < 0 || k >= 8) {
                        fprintf(stderr, "%s: invalid k number %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), arch);
                        res = 0;
			break;
		}
		while (*cp == ' ' || *cp == '\n')
			cp++;
		version = strtol(cp, &cp, 0);
		if (version < 0 || version >= (1<<24)) {
                        fprintf(stderr, "%s: invalid architecture version %d\n", hdr(file, line, &tmp[0], sizeof(tmp)), arch);
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
		if (!set_suota_upload(k, arch, version, cp)) {
                        fprintf(stderr, "%s: loading file '%s' failed\n", hdr(file, line, &tmp[0], sizeof(tmp)), cp);
                        res = 0;
		}
		break;
	default:
		fprintf(stderr, "%s: unknown command '%s'\n", hdr(file, line, &tmp[0], sizeof(tmp)), cp-1);
		res = 0;
		break;
	}
	return res;
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
rf_interface::set_suota_upload(int key, unsigned char arch, unsigned long version, const char *file)
{
	FILE *f = fopen(file, "r");
	if (!f)
		return 0;
	int total = 0;
	bool first = 1;
	for(;;) {
	    	int c;
            	int i;
            	int len, addr;
		c = fgetc(f);
		if (c < 0)
			break;
            	if (c == '\n')
                    continue;
            	if (c != ':') {
                    if (first) {
fail1:
			fclose(f);
                        return 0;
                    }
                    break;
            	}
            	len = hex(f);
            	if (len < 0)
                    	goto fail1;
            	addr = hex(f);
            	if (addr < 0)
                    	goto fail1;
            	i = hex(f);
            	if (i < 0)
                    	goto fail1;
            	addr = (addr<<8)+i;
            	int type = hex(f);
            	if (type < 0)
                    	goto fail1;
            	if (type == 0x01)
                    	break;
            	if (first) 
                	first = 0;
            	for (i = 0; i < len; i++) {
                	int v = hex(f);
                	if (v < 0)
                        	break;
            	}
            	i = hex(f);
            	if (i < 0)
                	goto fail1;
            	addr += len;
            	if (addr > total)
                    	total = addr;
        }
	if (total < 0 || total >= 65536) {
		fclose(f);
		return 0;
	}
	unsigned char *data = (unsigned char *)malloc(total+10);
	if (!data) {
		fclose(f);
		return 0;
	}
	memset(data, 0xff, total+10);
	rewind(f);
	first = 1;
	for(;;) {
	    	int c;
            	int i;
            	int sum=0, len, addr;
		c = fgetc(f);
		if (c < 0)
			break;
            	if (c == '\n')
                    continue;
            	if (c != ':') {
                    if (first) {
fail2:
			fclose(f);
			free(data);
                        return 0;
                    }
                    break;
            	}
            	len = hex(f);
            	if (len < 0)
                    	goto fail2;
            	sum += len;
            	addr = hex(f);
            	if (addr < 0)
                    	goto fail2;
            	sum += addr;
            	i = hex(f);
            	if (i < 0)
                    	goto fail2;
            	sum += i;
            	addr = (addr<<8)+i;
            	int type = hex(f);
            	if (type < 0)
                    	goto fail2;
            	sum += type;
            	if (type == 0x01)
                    	break;
            	if (first) 
                	first = 0;
            	for (i = 0; i < len; i++) {
                	int v = hex(f);
                	if (v < 0)
                        	break;
			if ((addr+i) >= total)
				break;
                	data[(addr+i)&0xffff] = v;
                	sum += v;
            	}
            	i = hex(f);
            	if (i < 0)
                	goto fail2;;
            	if (i != ((0x100-(sum&0xff))&0xff)) 
			goto fail2;
            	addr += len;
            	if (addr > total)
                    	total = addr;
        }
	fclose(f);
	
	suota_upload *sp = (suota_upload *)malloc(sizeof(*sp));
	if (!sp) {
		free(data);
		return 0;
	}
	data[4] = arch;
	data[5] = version;
	data[6] = version>>8;
	data[7] = version>>16;
	data[8] = total+10;
	data[9] = (total+10)>>8;
	unsigned long crc = crc32(crc32(0L, Z_NULL, 0), &data[4], total+6);
	data[0] = crc;
	data[1] = crc>>8;
	data[2] = crc>>16;
	data[3] = crc>>24;
	sp->len = total+10;
	sp->data = data;
	sp->key = key;
	sp->arch = arch;
	sp->version = version;
	sp->next = suota_list;
	suota_list = sp;
	return  1;
} 
