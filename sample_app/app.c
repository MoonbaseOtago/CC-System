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

#define FLASH_LED
#ifdef FLASH_LED
#ifdef VER
__bit led_state = 0;
static void led_update(task __xdata * t);
__xdata task led_task = {led_update,0,0,0};
static void led_update(task __xdata * t)
{
	led_state = !led_state;
	P2_0 = led_state;
	queue_task(&led_task, VER*HZ/3);
}
#endif
#endif


__xdata unsigned char mac[] = {0x84, 0x2b, 0x2b, 0x83, 0xaa, 0x07, 0x55, 0xaa};	// paul's laptop ether - will never xmit on wireless - expanded to 8 bytes
__code unsigned char ckey[] = {0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,	// 16 byte crypto key - will change
			       0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf};
__xdata unsigned char test[] = {1,2,3,4,5,6};


static void suota_broadcast(task __xdata * t);
__xdata task suota_broadcast_task = {suota_broadcast,0,0,0};

__data u8 led_ind=0;
__xdata u8 leds[6];

#define MAX_UNIQ	4
__pdata u8 uniq_filter[MAX_UNIQ*2];
__pdata u8 uniq_index;
unsigned char my_app(unsigned char op) 
{
	switch (op) {
	case APP_SUOTA_DONE:
		// do whatever you need to do to recover state then fall thru into APP_INIT
	case APP_INIT:
		uart_init();
#ifdef VV
		putstr(VV);
#else
		putstr("Hello World 2\r\n");
#endif
#ifdef DRV_LEDS
		leds_off();
#endif
#ifdef DRV_KEYS
		keys_on();	// call to enable key scanning (messes with uart)
#endif
		
		// pinMode(P(1,2), OUTPUT);
		// digitalWrite(P(1,2), HIGH);
		rf_set_channel(11);
		//rf_send((packet __xdata*)&test[0], 6, 0, 0);
		suota_allow_any_code = 0;	// use to allow any code_base to upgrade
		suota_key_required = 0;		// enable per-app download key if defined
		suota_enabled = 1;		// enable SUOTA
		queue_task(&suota_broadcast_task, 10);// since we're participating in suota we must
						// advertise what we have available - if we're not
						// broadcasting something else regularly
						// we need to set up a regular null broadcast
						// the rf subsystem will fill in all the interesting
						// bits in the packet header

#ifdef FLASH_LED
#ifdef VER
		P2DIR |= 1<<0;
		P2INP |= 1<<0;
`
		P2_0 = led_state;
		queue_task(&led_task, VER*HZ/3);
#endif
#endif
		break;
	case APP_GET_MAC:
		//rf_set_mac(&mac[0]);		// do this to set the mac
		return 0;			
	case APP_GET_KEY:
		rf_set_key_c(&ckey[0]);
		return 0;
	case APP_GET_SUOTA_KEY:
		// set up private SUOTA key if required, must have set suota_key_required at APP_INIT
		return 0;
	case APP_RCV_PACKET:
		//
		// for playa broadcast protocol first 3 bytes are of type broadcast_filter
		//
#ifdef NOTDEF
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
#endif
		// do something interesting here
		break;
	case APP_WAKE:
		break;
#ifdef DRV_KEYS
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
#ifdef DRV_LEDS
				leds_rgb(&leds[0]);
#endif
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
#ifdef DRV_LEDS
				leds_rgb(&leds[0]);
#endif
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
#endif
	case APP_SUOTA_START:
		return 0;	// 2 LSB:	0 means reboot after SUOTA
				// 		1 means no reboot, call init again
				// 		2 means no reboot, data has been saved call with APP_SUOTA_DONE to recover
				// if bit 2 is set the first 1k of the old code will be erased before
				//		control is passed to the new code - put your keys there
	}
	return 0;
}

static __xdata packet suota_packet;
static void
suota_broadcast(task __xdata * t)
{
	suota_packet.type = P_TYPE_NOP;
	suota_packet.arch = 0x12;
	suota_packet.code_base = 0x34;
	suota_packet.version[0] = 0x56;
	suota_packet.version[1] = 0x78;
	rf_send(&suota_packet, sizeof(suota_packet), SUOTA_KEY, 0);	// any key they will accept will do
	queue_task(&suota_broadcast_task, 10*HZ);
}
