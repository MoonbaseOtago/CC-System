// (c) Copyright Paul Campbell paul@taniwha.com 2013
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

#include <mcs51reg.h>
#include <cc2530.h>
#include "interface.h"

__xdata unsigned char mac[] = {0x84, 0x2b, 0x2b, 0x83, 0xaa, 0x07, 0x55, 0xaa};	// paul's laptop ether - will never xmit on wireless - expanded to 8 bytes
__xdata unsigned char ckey[] = {0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,	// 16 byte crypto key - will change
			       0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf};
__xdata unsigned char test[] = {1,2,3,4,5,6};


__data u8 led_ind=0;
__xdata u8 leds[6];

#define MAX_UNIQ	4
__pdata u8 uniq_filter[MAX_UNIQ*2];
__pdata u8 uniq_index;
unsigned int my_app(unsigned char op) 
{
	switch (op) {
	case APP_INIT:
		uart_init();
		putstr("Hello World\r\n");
		// something to initialise variables - needs compiler hack
		leds_off();
		// keys_on();	// call to enable key scanning (messes with uart)
		rf_set_channel(11);
		rf_send((packet __xdata*)&test[0], 6, 0, 0);
		break;
	case APP_GET_MAC:
		return (unsigned int)&mac[0];
	case APP_GET_KEY:
		rf_set_key(&ckey[0]);
		return 0;
	case APP_RCV_PACKET:
		//
		// for playa broadcast protocol first 3 bytes are of type broadcast_filter
		//
		if (rx_len < 3)
			return;
		if (rx_packet->data[0] > 0) {	// forward  packets
			u8 p0 = rx_packet->data[1];	// id
			u8 p1 = rx_packet->data[2];
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
			rx_packet->data[0]--;
			rf_send(rx_packet, rx_len, rtx_key, 0);
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

