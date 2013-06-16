#ifndef __interface_h
#define __interface_h

#include <mcs51reg.h>
#include <cc2530.h>
#include "task.h"

typedef struct packet {
	u8	type;
	u8	id[4];
	u8	arch;
	u8	version[3];
	u8	hops;
	u8	uniq[2];	// uniq filter
	u8	data[1];
} packet;

#define P_TYPE_NOP		0
#define P_TYPE_SUOTA_REQ	1
#define P_TYPE_SUOTA_RESP	2

#define P_TYPE_OTHERS		0x40 // writing your own code, want to add your own code
			     	     // allocate something above here

extern unsigned int (* __data x_app) (u8 v);
#define app (*x_app)
#define APP_INIT		0
#define APP_GET_MAC 		1
#define APP_GET_KEY 		2
#define APP_RCV_PACKET		3
extern u8 __pdata rx_len;
extern u8 __pdata rx_status[2];
extern packet __xdata  * __pdata rx_packet;
extern __bit rx_crypto;
#define APP_WAKE		4
#define APP_KEY			5
extern u8 __pdata key;
extern __bit key_down;
#define		KEY_X		0
#define		KEY_O		1
#define		KEY_LEFT	2
#define		KEY_RIGHT	3

#define THIS_ARCH	1	// initial CC2533
typedef struct code_hdr {
        unsigned long    crc;         
	unsigned char    arch;
        unsigned char    version[3];
        unsigned int     len;
        unsigned char	 data[1];
} code_hdr;

// call backs
extern void leds_rgb(unsigned char * __xdata);
extern void leds_off();
void rf_receive_on(void);
void rf_receive_off(void);
extern void rf_set_channel(u8 channel);
extern void rf_set_key(u8 __xdata *key);
void rf_set_mac(u8 __xdata *m);
extern void rf_send(packet __xdata* pkt, u8 len, u8 crypto);
extern unsigned char daylight();
extern void keys_ok();
extern void keys_off();
extern void putstr(char __code *cp);
extern void puthex(unsigned char v);


extern void (* __data uart_rx_0_vect) ();
extern void (* __data uart_rx_1_vect) ();
extern void (* __data uart_tx_0_vect) ();
extern void (* __data uart_tx_1_vect) ();
extern void (* __data p0_vect) ();
extern void (* __data p1_vect) ();
extern void (* __data p2_vect) ();
extern void (* __data t2_vect) ();
extern void (* __data t3_vect) ();
extern void (* __data t4_vect) ();
extern void (* __data adc_vect) ();
extern void (* __data aec_vect) ();
extern void (* __data dma_vect) ();
#endif
