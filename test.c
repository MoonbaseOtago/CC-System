// (c) Copyright Paul Campbell paul@taniwha.com 2013

#include <cc2530.h>
#include <stdio.h>
#include "interface.h"

unsigned char rgb[6];

void
delay(int ms)
{
	while (ms > 0) {
		int i;
		for (i = 0; i < 1000; i++)
			;
		ms--;
	}
}

void putchar(char c)
{
    U0DBUF = c;
    while (!UTX0IF)
	;
    UTX0IF = 0;
}

void putstr(char *cp)
{
	for (;;) {
		char c = *cp++;
		if (!c)
			return;
		if (c == '\n') {
			putchar(13);
			putchar(10);
		} else {
			putchar(c);
		}
	}
}

void
InitUart()
{
        PERCFG = 0;     // alt1 - USART0 on P0.3
        P0DIR |= 8;
        P0SEL |= 0x08;   // p0.8 out - TXD
        P2SEL = 0;
        P1SEL = 0;
        P2DIR = 0;
        U0CSR = 0x80;
        U0GCR = 0x08;
        U0BAUD = 59;
        U0CSR = 0x80;
        //U0UCR = 0x82;
        U0UCR = 0x86;
        UTX0IF = 0;
}

void
puthex(unsigned char v)
{
	unsigned char i = (v>>4)&0xf;
	if (i > 9) {
		putchar('a'+i-10);
	} else {
		putchar('0'+i);
	}
	i = v&0xf;
	if (i > 9) {
		putchar('a'+i-10);
	} else {
		putchar('0'+i);
	}
}
void
putv(unsigned char v)
{
	char c;
	if (v == 0) {
		putchar('0');
		return;
	}
	if (v > 99) {
		c = '0';
		while (v > 99) {
			c++;
			v -= 100;
		}
		putchar(c);
x:
		c = '0';
		while (v > 9) {
			c++;
			v -= 10;
		}
		putchar(c);
	} else
	if (v > 9)
		goto x;
	putchar(v+'0');
		
}

void
main()
{	
	unsigned char t=1, i;
	CLKCONCMD=0;	// 32MHz
	P1 = 0;
	P1DIR = 0xc1;
	P0DIR = 0x00;
	InitUart();
	delay(100);
	putstr("hello world\n");

#ifdef NOTDEF
	for(;;) {
		P1 = t|0x80;
		if (t == 0x20) {
			t = 0x01;
		} else {
			t = t<<1;
		}
	}
#endif
	#define PAD1 0x04
	#define PAD2 0x10
	#define PAD3 0x20
	#define PAD4 0x40
#ifdef TEST_PADS
	for (i = 0; ; i = (i+1)&3) {
		unsigned char r;
		unsigned long c;
	delay(1000);
putstr("inc = "); putv(i);putstr(" ");
		switch (i) {
		case 0: r = PAD1;
			P0INP = PAD1|PAD2;
			P0DIR |= PAD1|PAD2;	// drive both pads high
			P0 = PAD1|PAD2;
			delay(10);		// wait for them to charge
			P0DIR &= ~PAD1;		// stop driving pad1
			P0 &= ~(PAD1|PAD2);
			break;
		case 1: r = PAD2;
			P0INP = PAD1|PAD2;
			P0DIR |= PAD1|PAD2;
			P0 = PAD1|PAD2;
			delay(10);
			P0DIR &= ~PAD2;
			P0 &= ~(PAD1|PAD2);
			break;
		case 2: r = PAD3;
			P0INP = PAD3|PAD4;
			P0DIR |= PAD3|PAD4;
			P0 = PAD3|PAD4;
			delay(10);
			P0DIR &= ~PAD3;
			P0 &= ~(PAD3|PAD4);
			break;
		case 3: r = PAD4;
			P0INP = PAD3|PAD4;
			P0DIR |= PAD3|PAD4;
			P0 = PAD3|PAD4;
			delay(10);
			P0DIR &= ~PAD4;
			P0 &= ~(PAD3|PAD4);
			break;
		}
		for (c = 0; c < 65000; c++) {
			if (!(P0&r))
				break;
		}
		puthex(c>>8);
		puthex(c&0xff);
		putstr("\n");
		P0DIR &= ~(PAD1|PAD2|PAD3|PAD4);
	}
#endif
#ifdef TEST_DAYLIGHT
	P0=0;
	P0INP |= 0x80;
	P0DIR &= ~0x80;
	for (;;) {
		delay(100);
		if (P0&0x80) {
			putstr("dark\n");
		} else {
			putstr("light");
		}
	}
#endif
	P1DIR = 0xc1;
	P1INP = 0xc1;
	rgb[0] = 0x55;
	rgb[1] = 0xaa;
	rgb[2] = 0x33;
	rgb[3] = 0xcc;
	rgb[4] = 0x0f;
	rgb[5] = 0xf0;
//	for(;;) {
//		delay(10);
//		P1 = 0xc0;
//		delay(10);
//		P1 = 0x80;
//	}
	for(;;) {
		unsigned char i;
		for (i=0; i < 255; i++) {
			rgb[2] = i;
			rgb[1] = i;
			leds_rgb(&rgb[0]);
			delay(30);
		}
		for (i=255; i > 0; i--) {
			rgb[0] = i;
			rgb[5] = i;
			leds_rgb(&rgb[0]);
			delay(30);
		}
		for (i=0; i < 255; i++) {
			rgb[4] = i;
			rgb[3] = i;
			leds_rgb(&rgb[0]);
			delay(30);
		}
		for (i=255; i > 0; i--) {
			rgb[2] = i;
			rgb[1] = i;
			leds_rgb(&rgb[0]);
			delay(30);
		}
		for (i=0; i < 255; i++) {
			rgb[0] = i;
			rgb[5] = i;
			leds_rgb(&rgb[0]);
			delay(30);
		}
		for (i=255; i > 0; i--) {
			rgb[4] = i;
			rgb[3] = i;
			leds_rgb(&rgb[0]);
			delay(30);
		}
	}
}

