#include <mcs51reg.h>
#include <cc2530.h>
#include "interface.h"

static xxxx() __naked {
	__asm;
	.area CSEG    (CODE)
	.globl	_CODE_HEADER
_CODE_HEADER:
	.db	0, 0, 0, 0	// CRC will go here
	.db	THIS_ARCH
	.db	0, 0, 0		// version (little endian)
	.db	0, 0		// len from arch to end of code 
	ljmp	_my_app
	__endasm;
}

__code unsigned char mac[] = {0x84, 0x2b, 0x2b, 0x83, 0xaa, 0x07, 0x55, 0xaa};	// paul's laptop ether - will never xmit on wireless - expanded to 8 bytes
__code unsigned char ckey[] = {0x84, 0x2b, 0x2b, 0x83, 0xaa, 0x07, 0x55, 0xaa,	// 16 byte crypto key - will change
			       0x56, 0x87, 0x0f, 0x54, 0xd9, 0x2b, 0x76, 0x2e};

__data u8 led_ind=0;
__xdata u8 leds[6];

#define MAX_UNIQ	4
__pdata u8 uniq_filter[MAX_UNIQ*2];
__pdata u8 uniq_index;
static unsigned int my_app(unsigned char op) 
{
	switch (op) {
	case APP_INIT:
		// something to initialise variables - needs compiler hack
		leds_off();
		rf_set_channel(11);
		break;
	case APP_GET_MAC:
		return (unsigned int)&mac[0];
	case APP_GET_KEY:
		return (unsigned int)&ckey[0];
	case APP_RCV_PACKET:
		if (rx_packet->hops > 0) {	// forward  packets
			u8 p0 = rx_packet->uniq[0];
			u8 p1 = rx_packet->uniq[1];
			u8 __pdata *ph = &uniq_filter[0];
			u8 c;
			if (!rx_crypto)	// don't forward unverified packets
				break;
			c = MAX_UNIQ;
			while (c > 0) {
				if (*ph++ == p0) {
					if (*ph++ == p1) 
						return 0;	// discard duplicate
					
				} else {
					ph++;
				}
				c--;
			}
			rx_packet->hops--;
			rf_send(rx_packet, rx_len, 1);
			ph = &uniq_filter[uniq_index];
			*ph++ = p0;
			*ph = p1;
			uniq_index += 2;
			if (uniq_index >= (MAX_UNIQ*2))
				uniq_index = 0;
		}
		// do something interesting here
		break;
	case APP_WAKE:
		break;
	case APP_KEY:
		switch (key) {
		case KEY_O:
			putstr("O");
			if (key_down) {
				led_ind += 2;
				if (led_ind > 4)
					led_ind = 0;
			}
			break;
		case KEY_X:
			putstr("X");
			break;
		case KEY_LEFT:
			putstr("<");
			if (key_down) {
				if (leds[led_ind] >= 0x1f) {
					leds[led_ind] -= 0x20;
					leds[led_ind+1] -= 0x20;
				} else {
					leds[led_ind] = 0;
					leds[led_ind+1] = 0;
				}
				leds_rgb(&leds[0]);
			}
			break;
		case KEY_RIGHT:
			putstr(">");
			if (key_down) {
				if (leds[led_ind] < 0xe0) {
					leds[led_ind] += 0x20;
					leds[led_ind+1] += 0x20;
				} else {
					leds[led_ind] = 0xff;
					leds[led_ind+1] = 0xff;
				}
				leds_rgb(&leds[0]);
			}
			break;
		default:
			return 0;
		}
		if (key_down) {
			putstr(" pressed\n");
		} else {
			putstr(" released\n");
		}
		break;
	}
	return 0;
}

