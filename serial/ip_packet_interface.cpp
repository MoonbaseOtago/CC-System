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
#include "ip_packet_interface.h"
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
rf_handle
rf_open_ip(const char *serial_device, rf_rcv rcv_callback)
{
	ip_rf_interface *i = new ip_rf_interface(serial_device, rcv_callback);
	if (!i->opened_ok()) {
		delete i;
		return 0;
	}
	return (rf_handle)i;
}

void
rf_set_ip_enable(rf_handle handle, int on)
{
	ip_rf_interface *i = (ip_rf_interface*)handle;
	i->set_ip_enable(on);
}

ip_rf_interface::ip_rf_interface(const char *serial_device, rf_rcv rcv_callback):rf_interface(serial_device, rcv_callback)
{
	pthread_condattr_t ca;

	ip_list = 0;
	ip_lookup_list = 0;
	ip_enabled = 0;
	ip_lookup_busy = 0;
	ip_listen_busy = 0;
	pthread_mutex_init(&ip_mutex, 0);
        pthread_condattr_init(&ca);
        pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
	pthread_cond_init(&ip_cond, &ca);
	
}

ip_rf_interface::~ip_rf_interface()
{
	set_ip_enable(0);
	pthread_cond_destroy(&ip_cond);
	pthread_mutex_destroy(&ip_mutex);
}

bool
ip_rf_interface::virtual_command(const char *cc, const char *file, int line)
{
	char *cp = (char *)cc;	// cast to make strol happy

	char c = *cp++;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	switch (*cp) {
	case 'I':
		set_ip_enable(*cp == '-'?0:1);
		break;
	default:
		return 0;
	}
	return 1;
}


int
ip_rf_interface::alloc_ip_tag() 	// called with ip_mutex
{
	int res;

	for(;;) {
		bool found = 0;
		ip_active *ip;

		res = (last_tag++)&0xff;
		for (ip = ip_list;ip;ip=ip->next)
		if (ip->tag == res) {
			found = 1;
			break;
		}
		if (!found)
			break;
	}
	return res;
}

bool
ip_rf_interface::data_receive(bool crypto, unsigned char key, unsigned char *mac, unsigned char *pp, int len)
{
	packet *p = (packet *)pp;
	switch (p->type) {
	case P_TYPE_IP_LOOKUP:
		{
			ip_lookup *ip = (ip_lookup *)malloc(sizeof(*ip));
			if (!ip)
				break;
			ip->next = 0;
			ip->crypto = crypto;
			ip->key = key;
			memcpy(&ip->mac[0], &mac[0], 8);
			ip->tag = p->data[0]|(p->data[1]<<8);
			int l = len-(sizeof(*p)-1)-2;
			if (l > sizeof(ip->name))
				l = sizeof(ip->name);
			strncpy(ip->name, (char*)&p->data[2], l);
			pthread_mutex_lock(&ip_mutex);
			if (ip_lookup_list) {
				ip_lookup_list_last->next = ip;
			} else {
				ip_lookup_list = ip;
			}
			ip_lookup_list_last = ip;
			if (ip_lookup_busy) {
				pthread_cond_broadcast(&ip_cond);
			} else {
				pthread_t tid;
				ip_lookup_busy = 1;
				pthread_create(&tid, 0, ip_listener_thread, this);
			}
			pthread_mutex_unlock(&ip_mutex);
		}
		break;
	case P_TYPE_IP_CONNECT_TCP:
		{
			ip_active *ip; 
			unsigned char buff[128];
			int tag, private_tag = p->data[2]|(p->data[3]<<8);
			int port = p->data[2]|(p->data[3]<<8);
			int ok = 0;
			unsigned int iaddr = p->data[5]|(p->data[6]<<8)|(p->data[7]<<16)|(p->data[8]<<24);
			int sockfd;

			if (p->data[4] != 4)
				goto fail;
			sockaddr_in addr;
	   		addr.sin_family = AF_INET;
   			addr.sin_addr.s_addr = htonl(iaddr);
   			addr.sin_port = htons(port);
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (sockfd == 0)
				goto fail;
			if (connect(sockfd,(struct sockaddr *)&addr,sizeof(addr)) < 0) {
				close(sockfd);
				goto fail;
			}
			ip = (ip_active*)malloc(sizeof(*ip)); 
			if (!ip) {
				close(sockfd);
				goto fail;
			}
			ip->udp = 0;
			ip->fd = sockfd;
			memcpy(&ip->mac[0], &mac[0], 8);
			pthread_mutex_lock(&ip_mutex);
			ip->tag = tag = alloc_ip_tag();
			ip->next = ip_list;
			ip_list = ip;
			ip->crypto = crypto;
			ip->key = key;
			if (ip_listen_busy) {
				pthread_cond_broadcast(&ip_cond);
			} else {
				pthread_t tid;
				ip_listen_busy = 1;
				pthread_create(&tid, 0, ip_listener_thread, this);
			}
			pthread_mutex_unlock(&ip_mutex);
			ok = 1;
fail:
			p = (packet *)&buff[0];
			p->type = P_TYPE_IP_OPEN_RESP;
			
			p->data[0] = private_tag;
			p->data[1] = private_tag>>8;
			p->data[2] = ok;
			p->data[3] = tag;
			p->data[4] = tag>>8;
			if (crypto) {
				send_crypto(key, mac, (unsigned char *)p, sizeof(*p)-1+5);
			} else {
				send(mac, (unsigned char *)p, sizeof(*p)-1+5);
			}
		}
		break;
	case P_TYPE_IP_OPEN_UDP:
		{
			ip_active *ip; 
			unsigned char buff[128];
			int tag, private_tag = p->data[0]|(p->data[1]<<8);
			int ok = 0;
			int sockfd;
			sockaddr_in addr;
			socklen_t slen;
			int port = 0;
			int ipa = 0;

	   		addr.sin_family = AF_INET;
   			addr.sin_addr.s_addr = htonl(INADDR_ANY);
   			addr.sin_port = htons(0);
			sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (sockfd == 0)
				goto fail2;
			if (bind(sockfd,(struct sockaddr *)&addr,sizeof(addr)) < 0) {
				close(sockfd);
				goto fail2;
			}
			ip = (ip_active*)malloc(sizeof(*ip)); 
			if (!ip) {
				close(sockfd);
				goto fail2;
			}
			slen = sizeof(addr);
			ip->udp = 1;
			ip->fd = sockfd;
			memcpy(&ip->mac[0], &mac[0], 8);
			pthread_mutex_lock(&ip_mutex);
			ip->tag = tag = alloc_ip_tag();
			ip->next = ip_list;
			ip_list = ip;
			ip->crypto = crypto;
			ip->key = key;
			if (ip_listen_busy) {
				pthread_cond_broadcast(&ip_cond);
			} else {
				pthread_t tid;
				ip_listen_busy = 1;
				pthread_create(&tid, 0, ip_listener_thread, this);
			}
			pthread_mutex_unlock(&ip_mutex);
			ok = 1;
			slen = sizeof(addr);
			getsockname(sockfd, (struct sockaddr *)&addr, &slen);
			port = ntohs(addr.sin_port);
			ipa = ntohl(addr.sin_addr.s_addr);
fail2:
			p = (packet *)&buff[0];
			p->type = P_TYPE_IP_OPEN_RESP;
			p->data[0] = private_tag;
			p->data[1] = private_tag>>8;
			p->data[2] = ok;
			p->data[3] = tag;
			p->data[4] = tag>>8;
			p->data[5] = port;
			p->data[6] = port>>8;
			p->data[7] = 4;
			p->data[8] = ipa;
			p->data[9] = ipa>>8;
			p->data[10] = ipa>>16;
			p->data[11] = ipa>>24;
	
			if (crypto) {
				send_crypto(key, mac, (unsigned char *)p, sizeof(*p)-1+12);
			} else {
				send(mac, (unsigned char *)p, sizeof(*p)-1+12);
			}
		}
		
	case P_TYPE_IP_SEND:
		{
			ip_active *ip; 
			int tag = p->data[0]|(p->data[1]<<8);	
			pthread_mutex_lock(&ip_mutex);
			for (ip = ip_list; ip; ip = ip->next)
			if (ip->tag == tag) {
				if (ip->udp) {
					int port = p->data[2]|(p->data[3]<<8);
					if (p->data[4] != 4)
						break;
					unsigned int iaddr = p->data[5]|(p->data[6]<<8)|(p->data[7]<<16)|(p->data[8]<<24);
					sockaddr_in addr;
	   				addr.sin_family = AF_INET;
   					addr.sin_addr.s_addr = htonl(iaddr);
   					addr.sin_port = htons(port);
					sendto(ip->fd, &p->data[2], len-(sizeof(*p)-1)-2, MSG_DONTWAIT, 
						(const sockaddr*)&addr, sizeof(addr));
				} else {
					write(ip->fd, &p->data[2], len-(sizeof(*p)-1)-2);
				}
				break;
			}
			pthread_mutex_unlock(&ip_mutex);
		}
		break;
	case P_TYPE_IP_CLOSE:
		{
			ip_active *ip, *last = 0;
			int tag = p->data[0]|(p->data[1]<<8);	
			pthread_mutex_lock(&ip_mutex);
			for (ip = ip_list; ip; ip = ip->next) 
			if (ip->tag == tag) {
				close(ip->fd);
				if (last) {
					last->next = ip->next;
				} else {
					ip_list = ip->next;
				}
				free(ip);
			}
			pthread_mutex_unlock(&ip_mutex);
			unsigned char buff[128];
			p = (packet *)&buff[0];
			p->type = P_TYPE_IP_CLOSE_RESP;
			p->data[0] = tag;
			p->data[1] = tag>>8;
			if (crypto) {
				send_crypto(key, mac, (unsigned char *)p, sizeof(*p)-1+2);
			} else {
				send(mac, (unsigned char *)p, sizeof(*p)-1+2);
			}
		}
		break;
	default:
		return 0;
	}
	return 1;
}

void
ip_rf_interface::ip_lookup_thread()
{
	int res;

	pthread_mutex_lock(&ip_mutex);
	for(;;) {
		if (!ip_enabled)
			break;
		if (ip_lookup_list) {
			struct hostent *hp, h;
			char buff[1024];
			unsigned char b[128];
			packet *p = (packet *)&b[0];
			int len;
			int err;

			ip_lookup *ip = ip_lookup_list;
			ip_lookup_list = ip->next;
			pthread_mutex_unlock(&ip_mutex);
			
   			res = gethostbyname_r(ip->name, &h, buff, sizeof(buff), &hp, &err);
			if (!res && hp && hp->h_addr_list && hp->h_addr_list[0]) {
				p->type = P_TYPE_IP_LOOKUP_RESP;
				p->data[0] = ip->tag;
				p->data[1] = ip->tag>>8;
				p->data[2] = hp->h_addrtype;
				p->data[3] = hp->h_length;
				len = sizeof(*p)-1+4+hp->h_length;
			} else {
				p->type = P_TYPE_IP_LOOKUP_RESP;
				p->data[0] = ip->tag;
				p->data[1] = ip->tag>>8;
				p->data[2] = 0;
				p->data[3] = 0;
				len = sizeof(*p)-1+4;
			}
			memcpy(&p->data[2], hp->h_addr_list[0], hp->h_length);
			if (ip->crypto) {
				send_crypto(ip->key, ip->mac, (unsigned char *)p, len);
			} else {
				send(ip->mac, (unsigned char *)p, len);
			}
			free(ip);
			pthread_mutex_lock(&ip_mutex);
			continue;
		}
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		ts.tv_sec += 120;	//	
		for (;;) {
			res = pthread_cond_timedwait(&ip_cond, &ip_mutex, &ts);
			if (res == ETIMEDOUT || res == 0)
				break;
			if (errno != EAGAIN && errno != EINTR)
				break;
		}
		if (res == ETIMEDOUT)
			break;
	}
	ip_lookup_busy = 0;
	pthread_mutex_unlock(&ip_mutex);
}

void *
ip_rf_interface::ip_lookup_thread(void *p)
{
	ip_rf_interface *rp = (ip_rf_interface*)p;
	pthread_detach(pthread_self());
	rp->ip_lookup_thread();
	return 0;
}

void
ip_rf_interface::set_ip_enable(bool on)
{
	if (on) {
		ip_enabled = 1;
	} else {
		ip_active *ip;
		ip_lookup *ipl;

		pthread_mutex_lock(&ip_mutex);
		ip_enabled = 0;
		pthread_cond_signal(&ip_cond);
		while (ip_lookup_busy && ip_listen_busy) 
			pthread_cond_wait(&ip_cond, &ip_mutex);
		while (ip_list) {
			ip = ip_list;
			ip_list = ip->next;
			free(ip);
		}
		while (ip_lookup_list) {
			ipl = ip_lookup_list;
			ip_lookup_list = ipl->next;
			free(ipl);
		}
		pthread_mutex_unlock(&ip_mutex);
	}
}

void 
ip_rf_interface::ip_listener_thread()
{
	fd_set rlist;
	fd_set wlist;
	fd_set elist;

	FD_ZERO(&wlist);
	pthread_mutex_lock(&ip_mutex);
	for (;;) {
		ip_active *ip;

		if (!ip_enabled)
			break;
		if (!ip_list) {
			int res;
			struct timespec ts;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			ts.tv_sec += 120;	//	
			for (;;) {
				res = pthread_cond_timedwait(&ip_cond, &ip_mutex, &ts);
				if (res == ETIMEDOUT || res == 0)
					break;
				if (errno != EAGAIN && errno != EINTR)
					break;
			}
			if (res == ETIMEDOUT)
				break;
			continue;
		}
		pthread_mutex_unlock(&ip_mutex);
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		int nfds = 0;
		FD_ZERO(&rlist);
		FD_ZERO(&elist);
		for (ip = ip_list; ip; ip=ip->next) {
			FD_SET(ip->fd, &rlist);
			FD_SET(ip->fd, &elist);
			if (ip->fd >= nfds)
				nfds = ip->fd+1;
		}
		int n = select(nfds, &rlist, 0, &elist, &tv);
		if (!ip_enabled) {
			pthread_mutex_lock(&ip_mutex);
			if (!ip_enabled) 
				break;
			pthread_mutex_unlock(&ip_mutex);
		}
		if (n >= 0)
		for (ip = ip_list; ip; ip=ip->next) 
		if (FD_ISSET(ip->fd, &rlist)) {
			unsigned char b[128];
			packet *p = (packet *)&b[0];
			int len;
			if (ip->udp) {
				sockaddr addr;
				socklen_t s = sizeof(addr);
				
				sockaddr_in *ipa  = (sockaddr_in*)&addr ;
				int i = recvfrom(ip->fd, &p->data[9], 64, MSG_DONTWAIT, &addr, &s);
	   			if (ipa->sin_family != AF_INET)
					continue;
   				int ipv = ntohl(ipa->sin_addr.s_addr);
   				int port = ntohs(ipa->sin_addr.s_addr);
				p->type = P_TYPE_IP_RECV;
				p->data[0] = ip->tag;
				p->data[1] = ip->tag>>8;
				p->data[2] = port;
				p->data[3] = port>>8;
				p->data[4] = 4;
				p->data[5] = ipv;
				p->data[6] = ipv>>8;
				p->data[7] = ipv>>15;
				p->data[8] = ipv>>24;
				len = sizeof(*p)-1+2+i;
			} else {
				int i = read(ip->fd, &p->data[2], 64);
				p->type = P_TYPE_IP_RECV;
				p->data[0] = ip->tag;
				p->data[1] = ip->tag>>8;
				len = sizeof(*p)-1+2+i;
			}
			if (ip->crypto) {
				send_crypto(ip->key, ip->mac, (unsigned char *)p, len);
			} else {
				send(ip->mac, (unsigned char *)p, len);
			}
		}
		pthread_mutex_lock(&ip_mutex);
	}
	ip_listen_busy = 0;
	pthread_mutex_unlock(&ip_mutex);
}

void *
ip_rf_interface::ip_listener_thread(void *p)
{
	ip_rf_interface *rp = (ip_rf_interface*)p;
	pthread_detach(pthread_self());
	rp->ip_listener_thread();
	return 0;
}

