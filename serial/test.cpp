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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "packet_interface.h"

bool on=0;
int key=0;
bool mac_set=0;
unsigned char mac[8];
unsigned char xx[64];

void
help()
{
	printf("a: autodump\n");
	printf("A: autodump, raw and promiscuous\n");
	printf("c: set rf channel 11-26             - c channel\n");
	printf("h: help\n");
	printf("i: initialise file\n");
	printf("m: set mac                          - m a:b:c:d:e:f:g\n");
	printf("K: load key (16 hex bytes)          - K key-num value \n");
	printf("O: receiver on\n");
	printf("o: receiver off\n");
	printf("p: ping\n");
	printf("P: set promiscuous (snoops any packet)\n");
	printf("r: set raw - dumps all received pckets\n");
	printf("R: reset remote board\n");
	printf("s: send a broadcast packet          - s * cmd count\n");
	printf("s: send a directed packet           - s a:b:c:d:e:f:g cmd count\n");
	printf("!: send a broadcast crypto packet   - ! key-num * cmd count\n");
	printf("!: send a directed crypto packet    - ! key-num a:b:c:d:e:f:g cmd count\n");
	printf("U: rpt snd packet every N secs	    - U secs key-num arch code_base version\n");
	printf("u: suota update listener            - u arch code_base version file\n");
	printf("q: quit\n");
}

int
main(int argc, char **argv)
{
	rf_interface *rfp;
	const char *tp;
	char b[256];
	char *init=0;

	while (argc >= 2 && argv[1] && argv[1][0] =='-') {
		switch (argv[1][1]) {
		case 'i':
			if (argv[1][2]) {
				init = &argv[1][2];
			} else
			if (argc > 3) {
				argc--;
				argv++;
				init = argv[1];
			}
			break;
		default:
			fprintf(stderr, "Unknown flag '%s'\n", argv[1]);
			exit(99);
		}
		argc--;
		argv++;
	}
	if (argc < 2) {
		tp = "/dev/ttyUSB0";
	} else {
		tp = argv[1];
	}
	rfp = new rf_interface(tp, 0);
	if (!rfp->opened_ok()) {
		if (argc < 2) {
			delete rfp;
			rfp = new rf_interface("/dev/ttyACM1", 0);
			if (rfp->opened_ok()) 
				goto ok;
		}
		fprintf(stderr, "%s: failed to open '%s'\n", argv[0], tp);
		exit(99);
	}
ok:
	if (init) {	// non interactive versions
		rfp->initialise(init);
		for (;;)
			sleep(100);
	} else {
		help();
		for (;;) {
			char b[256];
			int c;
			char *cp;
			int i;
	
			printf("> ");
			if (!fgets(&b[0], sizeof(b), stdin))
				break;
			i = strlen(b);
			if (i > 0 && b[i-1] == '\n')
				b[i-1] = 0;
			cp = &b[0];
			while (*cp == ' ' || *cp == '\t')
				cp++;
			if (!cp)
				break;
			c = *cp;
			switch (c) {
			case 'q':
				goto quit;
			case 'i':
				cp++;
				while (*cp == ' ' || *cp =='\t')
					cp++;
				i = strlen(cp);
				while (i > 0 && (cp[i-1] == '\n' || cp[i-1] == ' ' || cp[i-1] == '\t')) {
					i--;
					cp[i] = 0;
				}
				if (!cp)
					break;
				rfp->initialise(cp);
				break;
			case '?':
			case 'h':	
				help();
				break;
			default:
				rfp->command(cp);
				break;
			}
		}
	}
quit:
	delete rfp;
	return 0;
}
