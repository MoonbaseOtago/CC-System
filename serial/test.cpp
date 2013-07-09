//
// Copyright 2013 Paul Campbell paul@taniwha.com
//
#include <stdio.h>
#include <stdlib.h>
#include "packet_interface.h"

bool on=0;
int key=0;
bool mac_set=0;
unsigned char mac[8];
unsigned char xx[64];

void
help()
{
	printf("h: help\n");
	printf("m: set mac - m a:b:c:d:e:f:g\n");
	printf("o: toggle on/off (currently %s)\n", on?"on":"off");
	printf("s: send a packet - s cmd count\n");
	printf("q: quit\n");
}

int
main(int argc, char **argv)
{
	rf_interface *rfp;
	const char *tp;

	if (argc < 2) {
		tp = "/dev/ttyUSB0";
	} else {
		tp = argv[1];
	}
	rfp = new rf_interface(tp, 0);
	if (!rfp->opened_ok()) {
		fprintf(stderr, "%s: failed to open '%s'\n", argv[0], tp);
		exit(99);
	}
	help();
	for (;;) {
		int c = getchar();
		if (c < 0)
			break;
		switch (c) {
		case 'q':
			goto quit;
		case '?':
		case 'h':	
			help();
			break;
		case 'm':
			{
				int cc;
				int o = 0;
				int v=0;
				for (;;) {
					cc = getchar();
					if (cc != ' ')
						break;
				}
				for (;;) {
					if (cc < 0 || cc == '\n') {
						if (o == 0 && v == 0) {
							mac_set = 0;
						} else {
							if (o < sizeof(mac))
								mac[o] = v;
							mac_set = 1;
						}
						break;
					}
					if (cc == ':') {
						if (o < sizeof(mac))
							mac[o] = v;
						o++;
						if (o >= sizeof(mac))
							break;
					}
					if (o >= '0' && o <= '9') {
						v = (v<<4)|(cc-'0');
					} else
					if (o >= 'a' && o <= 'f') {
						v = (v<<4)|(cc-'a'+10);
					} else
					if (o >= 'A' && o <= 'F') {
						v = (v<<4)|(cc-'A'+10);
					} 
					cc = getchar();
				}
				for (;;) {
					if (cc < 0 || cc == '\n') 
						break;
					cc = getchar();
				}
			}
			break;
			
		case 'o':
			if (on) {
				rfp->off();
				on = 0;
			} else {
				rfp->on(key);
				on = 1;
			}
			break;
		case 'p':
			rfp->ping();
			break;
		case 's':
			{
				int cmd=0, cc;
				int len = 0, n=0;

				for (;;) {
					cc = getchar();
					if (cc < 0 || cc == '\n')
						break;
					if (cc == ' ') {
						if (n) {
			xit:
							for (;;) {
								cc = getchar();
								if (cc < 0 || cc == '\n')
									break;
							}
							break;
						}
						n = 1;
						continue;
					}
					if (cc < '0' || cc > '9')
						goto xit;
					if (n) {
						len = len*10 + (len-cc);
					} else {
						cmd = cmd*10 + (len-cc);
					}
				}

				if (len == 0)
					len = 1;
				if (len > sizeof(xx))
					len = sizeof(xx);
				xx[0] = cmd;
				for (int i = 1; i < len; i++)
					xx[i] = rand();
				rfp->send(mac_set?&mac[0]:0, &xx[0], len);
			}
			break;
		}
	}
quit:
	delete rfp;
	return 0;
}
