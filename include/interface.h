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

#ifndef __interface_h
#define __interface_h

#include <mcs51reg.h>
#include <cc2530.h>
#include "task.h"
#include "protocol.h"
#include "suota.h"

extern unsigned int (* __data x_app) (u8 v);
#define app (*x_app)
#define APP_INIT		0
#define APP_GET_MAC 		1
#define APP_GET_KEY 		2
#define APP_RCV_PACKET		3
#define APP_LOW_LEVEL_INIT	0xff
extern u8 __data rx_len;
extern packet __xdata  * __data rx_packet;
__bit __at (0x00) rx_crypto;
__bit __at (0x01) rx_broadcast;
extern u8 __xdata * __data rx_mac;
#define APP_WAKE		4
#define APP_KEY			5
extern u8 __pdata key;
__bit __at (0x02) key_down;
#define		KEY_X		0
#define		KEY_O		1
#define		KEY_LEFT	2
#define		KEY_RIGHT	3

// call backs
extern void leds_rgb(unsigned char * __xdata);
extern void leds_off();
extern void rf_receive_on(void);
extern void rf_receive_off(void);
extern void rf_set_channel(u8 channel);
extern void rf_set_key(u8 __xdata *key);
extern u8 __data rtx_key;
extern void rf_set_mac(u8 __xdata *m);
#define NO_CRYPTO	0xff
#define SUOTA_KEY	0xfe
extern void rf_send(packet __xdata* pkt, u8 len, u8 crypto, __xdata unsigned char *mac);
#define	XMT_POWER_NEG_3DB	-3
#define	XMT_POWER_0DB		0
#define	XMT_POWER_4DB		4
#define	XMT_POWER_MAXDB		100
extern void rf_set_transmit_power(char power);
extern void rf_set_promiscuous(u8 on);
extern void rf_set_raw(u8 on);

extern unsigned char daylight(void);
extern void keys_on();
extern void keys_off();
extern void uart_init();
extern void putchar(char c);
extern void putstr(char __code *cp);
extern void puthex(unsigned char v);


extern void (* __pdata uart_rx_0_vect) ();
extern void (* __pdata uart_rx_1_vect) ();
extern void (* __pdata uart_tx_0_vect) ();
extern void (* __pdata uart_tx_1_vect) ();
extern void (* __pdata p0_vect) ();
extern void (* __pdata p1_vect) ();
extern void (* __pdata p2_vect) ();
extern void (* __pdata t2_vect) ();
extern void (* __pdata t3_vect) ();
extern void (* __pdata t4_vect) ();
extern void (* __pdata adc_vect) ();
extern void (* __pdata aec_vect) ();
extern void (* __pdata dma_vect) ();
#endif
