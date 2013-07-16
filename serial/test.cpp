//
// Copyright 2013 Paul Campbell paul@taniwha.com
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	printf("c: set rf channel 11-26             - c channel\n");
	printf("h: help\n");
	printf("i: initialise file\n");
	printf("m: set mac                          - m a:b:c:d:e:f:g\n");
	printf("k: on, set key                      - k key-num\n");
	printf("K: load key (16 hex bytes)          - K key-num value \n");
	printf("O: on, key 0\n");
	printf("o: off\n");
	printf("p: ping\n");
	printf("s: send a broadcast packet          - s * cmd count\n");
	printf("s: send a directed packet           - s a:b:c:d:e:f:g cmd count\n");
	printf("!: send a broadcast crypto packet   - ! key-num * cmd count\n");
	printf("!: send a directed crypto packet    - ! key-num a:b:c:d:e:f:g cmd count\n");
	printf("S: suota architecture for broadcast - S arch version\n");
	printf("u: suota update listener            - u arch version file\n");
	printf("q: quit\n");
}

int
main(int argc, char **argv)
{
	rf_interface *rfp;
	const char *tp;
	char b[256];

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
quit:
	delete rfp;
	return 0;
}
