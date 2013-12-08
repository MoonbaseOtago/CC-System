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

extern unsigned char (* __data x_app) (u8 v);
#define APP_INIT		0
#define APP_GET_MAC 		1
#define APP_GET_KEY 		2
#define APP_GET_SUOTA_KEY 	3
#define APP_RCV_PACKET		4
#define APP_LOW_LEVEL_INIT	0xff
extern u8 __data rx_len;
extern packet __xdata  * __data rx_packet;
__bit __at (0x00) rx_crypto;
__bit __at (0x01) rx_broadcast;
extern u8 __xdata * __data rx_mac;
#define APP_WAKE		5
#define APP_KEY			6
#define APP_SUOTA_START		7
#define	APP_SUOTA_DONE		8
__bit __at (0x03) suota_key_required;
__bit __at (0x04) suota_enabled;
__bit __at (0x05) suota_allow_any_code;
__bit __at (0x06) sys_active;	
__bit __at (0x07) suota_inprogress;	
#define		KEY_X		0
#define		KEY_O		1
#define		KEY_LEFT	2
#define		KEY_RIGHT	3
unsigned char __xdata *suota_allocate_save_space(u8 v);
unsigned char suota_get_save_size();
unsigned char __xdata *suota_get_save_space();

// call backs
#ifdef DRV_LEDS
extern void leds_rgb(unsigned char * __xdata);
extern void leds_off();
#endif
extern void rf_receive_on(void);
extern void rf_receive_off(void);
extern void rf_set_channel(u8 channel);
extern void rf_set_key(u8 __xdata *key);
extern void rf_set_key_c(u8 __code * key);
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

#ifdef DRV_DAYLIGHT
extern unsigned char daylight(void);
#endif
#ifdef DRV_KEYS
__bit __at (0x02) key_down;
extern u8 __pdata key;
extern void keys_on();
extern void keys_off();
#endif
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

//
// Simple Arduino compatability macros
//
//	instead of pin numbers we use P(port,bit)
//	P(1,2) refers to pin  p1.2 
//
//	"digitalWrite(P(1,2), HIGH);"
//
#ifndef LOW
#define HIGH 1
#define LOW 0
#define P(p, b) p , b
#define __XP(p,b) P##p##_##b
#define __Xp(p,b) p
#define __Xb(p,b) b
#define digitalWrite(p , v) __XP(p)=(v)
#define digitalRead(p) __XP(p)
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define __XPINP(p) P##p##INP
#define __XPDIR(p) P##p##DIR
#define __PINP(p) __XPINP(p)
#define __PDIR(p) __XPDIR(p)
#define pinMode(p, m) if ((m)==OUTPUT) {__PDIR(__Xp(p)) |= (1<<__Xb(p)); __PINP(__Xp(p)) |= (1<<__Xb(p));} else { __PDIR(__Xp(p)) &= ~(1<<__Xb(p)); if ((m)==INPUT_PULLUP) {__PINP(__Xp(p)) &= ~(1<<__Xb(p)); } else {__PINP(__Xp(p)) |= (1<<__Xb(p));} }
#endif
#endif
