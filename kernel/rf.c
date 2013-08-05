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
#include <string.h>
#include "task.h"
#include "rf.h"
#include "interface.h"
#include "suota.h"

#define RX_DATA_SIZE 128
static __xdata u8 rx_data0[RX_DATA_SIZE];
static __xdata u8 rx_data1[RX_DATA_SIZE];
__bit rx_busy0;
__bit rx_busy1;
__bit rx_set;
__bit rx_rset;
static u8 __data rx_len0;
static u8 __data rx_len1;
u8 __data rx_len;
packet __xdata  * __data rx_packet;
u8 __xdata * __data rx_mac;
__bit rx_crypto;
__bit rx_broadcast;
static __bit rx_raw=0;
__xdata static u8 tmp[128];
__xdata static u8 cipher[128];
__pdata static u8 nonce_tx[16];
__pdata static u8 nonce_rx[16];
__pdata static u8 iv[16];
__pdata u8 rf_id[2];
__xdata unsigned char tmp_packet[32];
static u8 __pdata seq;
void rf_receive_on(void);
extern void putstr (const char __code*);
extern void puthex (unsigned char );
__xdata __at (0x616A)  u8 IEEE_MAC[8];
static void _aes_op();	// version with reg wrap;per

static __code unsigned char default_mac[] = {0x84, 0x2b, 0x2b, 0x83, 0xaa, 0x07, 0x55, 0xaa};	// paul's laptop ether - will never xmit on wireless - expanded to 8 bytes

//#define RDEBUG
#ifdef RDEBUG
extern void ps(const char __code *);
extern void pf(const char __code *);
extern void pd(unsigned char __xdata *, u8);
extern void pdd(unsigned char __pdata *, u8);
extern void ph(u8);
#else
#define ps(x)
#define ph(x)
#define pf(x)
#define pd(x, y)
#define pdd(x, y)
#endif
void
rf_set_key(u8 __xdata * key)
{
    u8 i;
ps("key=");pd(key, 16);pf("\n");
    ENCCS = (ENCCS & ~0x07) | 0x04;	// AES_LOAD_KEY
    ENCCS |= 0x01;			// start
    for (i=0; i<16; i++) 
        ENCDI = *key++;
}

void
rf_set_promiscuous(u8 on)
{
    FRMFILT0 = 0x0c|(on?0:1); // filtering on
}

void
rf_set_raw(u8 on)
{
    if (on) rx_raw = 1; else rx_raw = 0;
}

void
rf_set_mac(u8 __xdata *m)
{
	u8 i;
	for (i = 0; i < 6; i++)
		IEEE_MAC[i] = m[i];
	nonce_tx[8] = IEEE_MAC[6] = rf_id[0] = m[6];
	nonce_tx[7] = IEEE_MAC[7] = rf_id[1] = m[7];
}

void
rf_init(void)
{
    u8 i;

    rx_busy0 = 0;
    rx_busy1 = 0;
    rx_set = 0;
    rx_rset = 0;
    {
    	u8 __xdata *m = (u8 __xdata*)app(APP_GET_MAC);
    
	if (!m) {
		for (i = 0; i < 8; i++)
			IEEE_MAC[i] = default_mac[i];
	} else {
		for (i = 0; i < 8; i++)
			IEEE_MAC[i] = *m++;
    	}
    }
    // Enable auto crc
    FRMCTRL0 |= (1<<6);	

    // Recommended RX settings
    TXFILTCFG = 0x09;
    AGCCTRL1 = 0x15;
    FSCAL1 = 0x00;
    //IVCTRL = 0x0f;
    FSCTRL = 0x55;

    FRMFILT0 = 0x0d; // filtering on
    FRMFILT1 = 0x10; // data only
    SRCMATCH = 0;	// disable source matching

    RFIRQM0 |= 1<<6;	// RXPKTDONE
    IEN2 |= 1<<0;

    memset(&nonce_rx[0], 0, sizeof(nonce_rx));
    memset(&nonce_tx[0], 0, sizeof(nonce_tx));
    nonce_rx[0] = 9;
    nonce_rx[13] = 6;
    nonce_tx[0] = 9;
    nonce_tx[13] = 6;
    nonce_tx[8] = rf_id[0] = IEEE_MAC[6];
    nonce_tx[7] = rf_id[1] = IEEE_MAC[7];

    // generate rf_id  XXX
    
    
    app(APP_GET_KEY);

    rf_set_channel(11);	// for a start
    rf_receive_on();
}



void
rf_set_channel(u8 channel)
{
    FREQCTRL = 11+(channel-11)*5;
}

void
rf_set_transmit_power(char power)
{
	switch (power) {
	case -3:	FSCTRL = 0x55; TXCTRL = 0x69; TXPOWER = 0x9c; break;
	case  0:	FSCTRL = 0x55; TXCTRL = 0x69; TXPOWER = 0xbc; break;
	case  4:	FSCTRL = 0x55; TXCTRL = 0x69; TXPOWER = 0xec; break;
	case 100:	FSCTRL = 0xF5; TXCTRL = 0x74; TXPOWER = 0xfd; break;
	}
}

void
rf_receive_on(void)
{
	RFST = 0xec;
	RFST = 0xe3;
}

void
rf_receive_off(void)
{
	RFST = 0xef;
	RFST = 0xec;
}

#define AES_ENCRYPT     0x00
#define AES_DECRYPT     0x02
#define AES_LOAD_KEY    0x04
#define AES_LOAD_IV     0x06
#define AES_MODE_CBC    0x00
#define AES_MODE_CFB    0x10
#define AES_MODE_OFB    0x20
#define AES_MODE_CTR    0x30
#define AES_MODE_ECB    0x40
#define AES_MODE_CBCMAC 0x50

static void
aes_op(u8 __xdata* p, u8 op, u8 len, u8 __xdata *pOut, u8 __pdata*iv)
{
	u8 blocks, j, k, mode, b;

	blocks = (len+15)&0xf0;
	mode = ENCCS&~0x7;
	ENCCS = mode|AES_LOAD_IV;
	ENCCS |= 0x01;
ps("iv=");pdd(iv, 16);pf("\n");
	for(j = 0; j < 16; j++)
      		ENCDI = *iv++;

	ENCCS = mode|op;
	mode &= 0x70;

	if (mode==AES_MODE_CTR || mode==AES_MODE_CFB || mode==AES_MODE_OFB) {
ps("mode1=");ph(mode);ps(" len=");ph(len);pf("\n");
ps("data=");pd(p, len);pf("\n");
		for (b = 0; b < blocks; b+=16) {
			ENCCS |= 0x01;
			for (j=0; j < 16; j+=4) {
            			for (k=0; k < 4; k++) 
               				ENCDI = (b+j+k) < len?p[b+j+k]:0x00;
				*pOut++ = ENCDO;
				*pOut++ = ENCDO;
				*pOut++ = ENCDO;
				*pOut++ = ENCDO;
			}
		}
ps("out=");pd(pOut-b, b);pf("\n");
	} else
	if (mode == AES_MODE_CBCMAC){
ps("mode2=");ph(mode);pf("\n");
ps("data=");pd(p, len);pf("\n");
		for (b = 0; b < blocks; b+=16) {
			ENCCS |= 0x01;
			for (j=0; j < 16; j++)
            			ENCDI = (b+j) < len?p[b+j]:0x00;
			if (b == (blocks-32)) {
				ENCCS &= ~0x70;
				ENCCS |= AES_MODE_CBC;
				wait_us(1);
			} else
			if (b == (blocks-16)) {
				wait_us(1);
				for (j=0; j < 16; j++)
					*pOut++ = ENCDO;
			}
		}
ps("out=");pd(pOut-16, 16);pf("\n");
	} else {
ps("mode3=");ph(mode);pf("\n");
ps("data=");pd(p, len);pf("\n");
		for (b = 0; b < blocks; b+=16) {
			ENCCS |= 0x01;
			for(j=0; j < 16; j++)
            			ENCDI = (b+j) < len?p[b+j]:0x00;
			wait_us(1);
			for(j=0; j < 16; j++)
				*pOut++ = ENCDO;
		}
ps("out=");pd(pOut-b, b);pf("\n");
	}
}

#define	LM  ((1<<((2&3)+1))&~3)
#define HDR_SIZE (2+1+2+2+8) 
static u8 __xdata* __data rx_data;
void
rcv_handler(task __xdata* ppp)
{
	u8 hdr;
// data points to
//	0:	2 fxf
//	2:	1 seq
//	3:	2 dst pan broadcast
//	5:	2 dst addr broadcast
//	7:	8 src addr 
//	15:	data 
//
// for crypto packets
//	15:	1 '6'
//	16:	4 frame counter
//	20:	data
//	n-8:	8 crypto
//

	for(;;) {
		if (rx_rset) {
			if (!rx_busy1)
				return;
			rx_data = &rx_data1[0];
			rx_len = rx_len1;
		} else {
			if (!rx_busy0)
				return;
			rx_data = &rx_data0[0];
			rx_len = rx_len0;
		}
		if (rx_data[1]&(1<<2)) {	// mac dest
			hdr = HDR_SIZE-2+8;
			rx_mac = &rx_data[5+8];
			rx_broadcast = 0;	
		} else {
			hdr = HDR_SIZE;
			rx_mac = &rx_data[5+2];
			rx_broadcast = 1;
		}
		if (rx_data[0]&(1<<3) && !rx_raw) { 	// encrypted?
			u8 c, i, l;
			u8 __xdata* p;
			u8 __xdata* p2;
			//
			// len = rx_len - header len -2 length of data + crypto
			// c = rx_len-5-8	// length of data
			// f = 15+5	// length up to start of data
			// m = 2
	//putchar('a');
ps("RECV=");pd(rx_data, rx_len);pf("\n");
			nonce_rx[9] = rx_data[hdr+1];
			nonce_rx[10] = rx_data[hdr+2];
			nonce_rx[11] = rx_data[hdr+3];
			nonce_rx[12] = rx_data[hdr+4];
			nonce_rx[8] = rx_mac[6];
			nonce_rx[7] = rx_mac[7];
			iv[0] = 1;
			memcpy(&iv[1], &nonce_rx[1], 14);
			iv[15] = 0;
	
	//putchar('b');
			ENCCS &= ~0x70;
			ENCCS |= AES_MODE_CTR;
			aes_op(&rx_data[rx_len-8], AES_DECRYPT, LM, tmp, iv);
	//putchar('c');
			memcpy(&rx_data[rx_len-8], tmp, LM);
	//putchar('d');
	
			c = rx_len-8-5-hdr;
	//putchar('e');
			memcpy(&cipher[0], &rx_data[hdr+5], c);
			p = &cipher[c];
			l = (c+15)&0xf0;
			for (i=c; i<l; i++)
				*p++ = 0; 
			iv[15]= 1;
			ENCCS &= ~0x70;
			ENCCS |= AES_MODE_CTR;
	//putchar('f');
			aes_op(&cipher[0], AES_DECRYPT, c, tmp, iv);
	//putchar('g');
			memcpy(&rx_data[hdr+5], &tmp[0], c);
	//putchar('h');
	
			tmp[0] = 0x41 | ((LM-2)/2)<<3;
			memcpy(&tmp[1], &nonce_rx[1], 13);
			tmp[14] = 0;
			tmp[15] = c;
			tmp[16] = 0;
			tmp[17] = hdr+5;
			memcpy(&tmp[18], &rx_data[0], hdr+5);
	//putchar('i');
			i = 18+hdr+5;
			l = (18+hdr+5+15)&0xf0;
			p = &tmp[i];
			while (i < l) {
				i++;
				*p++ = 0;
			}
	//putchar('j');
			p2 = &rx_data[hdr+5];
			l += c;
			while (i < l) {
				*p++ = *p2++;
				i++;
			}
	//putchar('k');
			l = ((i+15)&0xf0) - i;
			while (l) {
				*p++ = 0;
				l--;
			}
	//putchar('l');
			memset(iv,0, sizeof(iv));
			ENCCS &= ~0x70;
                	ENCCS |= AES_MODE_CBCMAC;
	//putchar('m');
                	aes_op(&tmp[0], AES_ENCRYPT, i, &cipher[0], &iv[0]);
	//putchar('n');
			if (memcmp(&cipher[0], &rx_data[rx_len-8], LM) != 0) {	// discard bogus packet
				return;
			}
			rx_crypto = 1;
			rx_packet = (packet __xdata *)&rx_data[hdr+5];
			rx_len = c;
			if (suota_enabled) {
				if (incoming_suota_version(rx_packet))
					return;
				if  (rx_packet->type == P_TYPE_SUOTA_REQ || rx_packet->type == P_TYPE_SUOTA_RESP) {
					incoming_suota_packet(rx_packet, rx_len);
					return;
				}
			}
		} else {
			rx_crypto = 0;
			rx_len -= hdr;
			rx_packet = (packet __xdata *)&rx_data[hdr];
		}
		app(APP_RCV_PACKET);
		if (rx_rset) {
			rx_busy1 = 0;
			rx_rset = 0;
		} else {
			rx_busy0 = 0;
			rx_rset = 1;
		}
	}
}

__xdata task rcv_task = {rcv_handler, 0, 0, 0};

void rf_isr()  __interrupt(16) __naked
{
   	__asm;
	// x = EA;
    	// EA = 0;
	push 	acc
	mov	a, _RFIRQF0
	jb	acc.6, 1001$ 	//	if(RFIRQF0&(1<<6)) { 		// RXPKTDONE
		ljmp	0001$
1001$:
	push	_PSW
	push	dpl
	push	dph
	push	_DPS
	mov	_DPS, #0
	push	ar0
	push	ar1
	mov	dptr, #_RFIRQM0		//	 RFIRQM0 &= ~(1<<6);		// disable RF ints RXPKTDONE
	movx	a, @dptr
	anl	a, #~(1<<6)
	movx	@dptr, a

	anl	_IEN2, #~(1<<0)		//	IEN2 &= ~(1<<0);
	//					EA = x;				// enable others`
	mov	a, _RFD			//	l = RFD;			// length
	anl	a, #0x7f		//	l &= 0x7f;
	add	a, #-2
	mov	r1, a			//	l -= 2;	// status
	jb	_rx_set, 0124$
	jb	_rx_busy0, 0002$ 	//	if (rx_busy || l <= 5 || l > sizeof(rx_data)) {			// already have a buffer waiting
0123$:	cjne	r1, #5, 0011$
		sjmp	0002$
0011$:	jc	0002$
	cjne	r1, #RX_DATA_SIZE, 0024$
		sjmp	0003$
0124$:	jnb      _rx_busy1, 0123$
		sjmp	0002$
0024$:	jc	0003$
0002$:		inc	r1		//		l += 2 // status	
		inc	r1
0022$:			mov	a, _RFD		//	while (l--) 
			djnz	r1, 0022$	//		t = RFD;
		ljmp	0005$		//	} else {
0003$:		
		jb	_rx_set, 0225$
			mov	dptr, #_rx_data0	//		p = &rx_data[0];
			mov	_rx_len0, r1	//		rx_len = l;
			sjmp	0004$
0225$:
			mov	dptr, #_rx_data1	//		p = &rx_data[0];
			mov	_rx_len1, r1	//		rx_len = l;
0004$:		
			mov	a, _RFD //		while (l--) 
			movx	@dptr, a//			*p++ = RFD;
			inc	dptr
			djnz	r1, 0004$
		mov	a, _RFD		//		rx_status[0] = RFD;
		mov	a, _RFD		//		rx_status[1] = RFD;
		jnb	acc.7, 0005$	//		if ((rx_status[1]&0x80)) { 	// CRC check
			jb	_rx_set, 0224$
				setb    _rx_busy0	//		rx_busy = 1;
				sjmp 0223$
0224$:				setb    _rx_busy1
0223$:			cpl     _rx_set
			push	ar2
			push	ar3
			push	ar4
			push	ar5
			push	ar6
			push	ar7
			push	_DPH1
			push	_DPL1
			mov	dptr, #_rcv_task	// 	 queue_task(&rcv_task, 0);	// wake the bottom half
			mov	r0, #_queue_task_PARM_2
			clr	a
			movx	@r0, a
			inc	r0
			movx	@r0, a
			lcall	_queue_task
			pop	_DPL1
			pop	_DPH1
			pop	ar7
			pop	ar6
			pop	ar5
			pop	ar4
			pop	ar3
			pop	ar2
							//	}
0005$:
		//					}
    	//EA = 0;
	mov	dptr, #_RFIRQM0		//	 RFIRQM0 |= ~(1<<6);		// enable RF ints RXPKTDONE
	movx	a, @dptr
	orl	a, #(1<<6)
	movx	@dptr, a

	orl	_IEN2, #1<<0		//	IEN2 |= (1<<0);		
        mov	_S1CON, #0		//	S1CON = 0;                    // Clear general RF interrupt flag
        anl	_RFIRQF0, #~(1<<6)	//	RFIRQF0 &= ~(1<<6);   // Clear RXPKTDONE interrupt
   					// }
	pop	ar1
	pop	ar0
	pop	_DPS
	pop	dph
	pop	dpl
	pop	_PSW
0001$:
    	//EA = x;
	pop	acc
	reti
	__endasm;
}


static __bit xmt_broadcast;
void
rf_send(packet __xdata *pkt, u8 len, u8 crypto, __xdata unsigned char *xmac) __naked
{
	__asm;
	mov	r6, dpl
	mov	r7, dph		// pkt
	mov	r0, #_rf_send_PARM_2
	movx	a, @r0
	mov	r5, a		// len
	jnb	_suota_enabled, 0700$	//	if (suota_enabled) {
		inc	dptr
		mov	r0, #_rf_id	//		memcpy(&pkt->id[0], &rf_id[0], 2);
		mov	r2, #2
0018$:			movx	a, @r0
			inc	r0
			movx	@dptr, a
			inc dptr
			djnz	r2, 0018$
		mov	r0, #_current_code//    	pkt->arch = current_code->arch;
		movx	a, @r0		//		memcpy(&pkt->version[0], &current_code->version[0], 3);
		add	a, #4
		mov	_DPL1, a
		inc 	r0
		movx	a, @r0
		addc	a, #0
		mov	_DPH1, a
		mov	r2, #4
0019$:			inc     _DPS    // = #1
			movx	a, @dptr
			inc	dptr
			dec	_DPS	// = #0
			movx	@dptr, a
			inc	dptr
			djnz	r2, 0019$
0700$:
	mov	dptr, #_FSMSTAT1//	while (FSMSTAT1 & ((1<<1) | (1<<5)))	// SFD | TX_ACTIVE
0017$:		movx	a, @dptr//		;
		anl	a, #(1<<1)|(1<<5)
		jnz	0017$
	mov	dptr, #_RFIRQM0	//	RFIRQM0 &= ~(1<<6);	// disable rx int	RXPKTDONE
	movx	a, @dptr
	anl	a, #~(1<<6)
	movx	@dptr, a

	anl	_IEN2, #~(1<<0)	//	IEN2 &= ~(1<<0);		// RFIE

	mov	_RFST, #0xee	//	RFST = 0xEE;	// txflush

	mov	_RFIRQF1, #~(1<<1)//	RFIRQF1 = ~(1<<1);		// IRQ_TXDONE
		
	//
	//	packetLength;
  	//	fcf0;           // Frame control field LSB
    	//	fcf1;           // Frame control field MSB
    	//	seqNumber;	00
    	//	panId;		ff ff
    	//	destAddr;	ff ff or 8 bytes of mac
    	//	srcAddr;	11 22 33 44 55 66 77 88
	// crypto
        //	securityControl;
    	//	frameCounter[4];
	mov	r0, #_rf_send_PARM_4	// mac pointer
	movx	a, @r0		//	if (!crypto) {
	mov	r3, a
	mov	r2, #0
	inc	r0
	movx    a, @r0
	mov	r4, a
	setb	_xmt_broadcast
	orl	a, r3
	jz	2012$
		clr	_xmt_broadcast
		mov	a, #8
2012$:
	mov	r2, a
	mov	r0, #_rf_send_PARM_3
	movx	a, @r0			//	if (!crypto) {
	jnz	0012$
		mov	a, r5		//		RFD = len+HDR_SIZE+0;
		add	a, #HDR_SIZE+2
		add	a, r2
		mov	_RFD, a
		mov	a, #(1<<0)|(1<<6)//	} else {
		sjmp	0013$

0012$:	 	mov	a, r5		//		RFD = len+HDR_SIZE+5;
		add	a, #HDR_SIZE+5+8+2
		add	a, r2
		mov	_RFD, a
		mov     a, #(1<<0)|(1<<6)|(1<<3)//}

0013$:
	mov	dptr, #_tmp+18		//	p = &tmp[18];
	mov	_RFD, a			//	RFD = *p++ =  	(1<<0)|	// DATA			fc0
	movx	@dptr, a		//	   		(crypto?(1<<3):(0<<3))|	// crypto
	inc	dptr			//	      		(0<<4)|	// frame pending
					//	      		(0<<5)|	// ack request 
					//	     		(1<<6);	// pan compression
	mov	a, r2
	rr	a	// 4 or 0  -> dest addressing 64-bit or 16-bit broadcast (3<<2)
	orl	a, #(2<<2)|(1<<4)|(3<<6)//	RFD = *p++ =	(2<<2)| // dest addressing (16-bit broadcast)
	mov	_RFD, a			//		    	(1<<4)|	// frame version
	movx	@dptr, a		//		      	(3<<6);	// source addressing 64-bit address
	inc	dptr
	mov	r0, #_seq		//	RFD = *p++ = seq++;
	movx	a, @r0
	inc	a
	movx	@r0, a
	mov	_RFD, a
	movx	@dptr, a
	inc	dptr

	jnb	_xmt_broadcast, 2013$
		mov	r2, #4		//	RFD = *p++ = 0xff;	// dest pan
		mov	a, #0xff	//	RFD = *p++ = 0xff;
0011$:			movx    @dptr, a//	RFD = *p++ = 0xff;	// dest broadcast
			inc	dptr	//	RFD = *p++ = 0xff;
			mov	_RFD, a
			djnz	r2, 0011$
		sjmp	2014$
2013$:
		mov	a, #0xff	//	RFD = *p++ = 0xff;
		movx    @dptr, a	//	RFD = *p++ = 0xff;	// dest broadcast pan
		inc	dptr
		movx    @dptr, a
		mov     _RFD, a
		mov     _RFD, a
		mov	r2, #8		//	RFD = *p++ = *xmac++;	// dest addres
				 	//	RFD = *p++ = *xmac++;
					//	RFD = *p++ = *xmac++;
		mov	_DPL1, r3	//	RFD = *p++ = *xmac++;
					//	RFD = *p++ = *xmac++;
					//	RFD = *p++ = *xmac++;
		mov	_DPH1, r4
2110$:			inc	_DPS// #1//	RFD = *p++ = *xmac++;
			clr	a
			movx	a, @dptr// 	RFD = *p++ = *xmac++;	
			inc 	dptr
			dec     _DPS// #0
			mov	_RFD, a
			movx	@dptr, a
			inc 	dptr
			djnz	r2, 2110$

2014$:
	mov	r2, #8		//	RFD = *p++ = *mac++;	// src addres
			 	//	RFD = *p++ = *mac++;
				//	RFD = *p++ = *mac++;
	mov	_DPL1, #_IEEE_MAC//	RFD = *p++ = *mac++;
				//	RFD = *p++ = *mac++;
				//	RFD = *p++ = *mac++;
	mov	_DPH1, #_IEEE_MAC>>8
0010$:		inc	_DPS// #1//	RFD = *p++ = *mac++;
		clr	a
		movx	a, @dptr// 	RFD = *p++ = *mac++;	
		inc 	dptr
		dec     _DPS// #0
		mov	_RFD, a
		movx	@dptr, a
		inc 	dptr
		djnz	r2, 0010$
	
	mov	r0, #_rf_send_PARM_3
	movx	a, @r0		//	if (crypto) {
	jnz	9006$
		ljmp	0006$
9006$:
		mov	a, #6
		mov	_RFD,a	//		RFD = 0x06;
		movx	@dptr, a
		inc	dptr
		
		mov	r0, #_nonce_tx+9//	RFD = nonce_tx[9];
		mov	r2, #4
0021$:			movx	a, @r0	//	RFD = nonce_tx[10];
			mov	_RFD, a	//	RFD = nonce_tx[11];
			movx	@dptr, a
			inc	dptr
			inc	r0
			djnz	r2, 0021$//	RFD = nonce_tx[12];
// set up nonce in crypto buffer

0023$:
		inc	_DPS// #1		// DP1 -> tmp
		mov	dptr, #_tmp		//	tmp[0] = 0x41 |
		mov	a, #0x41|(((LM-2)/2)<<3)//		 ((LM-2)/2)<<3;
		movx	@dptr, a
		inc	dptr
		mov	r2, #13			//	memcpy(&tmp[1], &nonce_tx[1], 13);
		mov	r0, #_nonce_tx+1;
0024$:			movx	a, @r0
			movx	@dptr, a
			inc	dptr
			inc	r0
			djnz    r2, 0024$
//
//	c = length 
//	f = HDR_SIZE+5
//	m = 2
//      lm=8
		clr	a		//	tmp[14] = 0;
		movx	@dptr, a
		inc	dptr
		mov	a, r5		//	tmp[15] = len;	// crypto len
		movx	@dptr, a
		inc	dptr
		clr	a		//	tmp[16] = 0;
		movx	@dptr, a
		inc	dptr
		jb	_xmt_broadcast, 5501$
			mov	a, #HDR_SIZE+5+8-2
			movx	@dptr, a	
			mov	dptr, #_tmp+(18+HDR_SIZE+5+8-2);
			mov 	r1, #((18+HDR_SIZE+5+8-2+15)&0xf0) //	i = 18+HDR_SIZE+5;
			mov	r2, #((18+HDR_SIZE+5+8-2+15)&0xf0)-(18+HDR_SIZE+5+8-2)  
			sjmp	5502$
5501$:
			mov	a, #HDR_SIZE+5	//	tmp[17] = HDR_SIZE+5;
			movx	@dptr, a	
			mov	dptr, #_tmp+(18+HDR_SIZE+5);
			mov 	r1, #((18+HDR_SIZE+5+15)&0xf0) //	i = 18+HDR_SIZE+5;
			mov	r2, #((18+HDR_SIZE+5+15)&0xf0)-(18+HDR_SIZE+5)
5502$:
//1003$:	jb	_tx_busy, 1003$
// mov	_U0DBUF, #'A'	//		U0DBUF = c;
//1004$:	jnb	_UTX0IF, 1004$
// clr	_UTX0IF
// mov	_U0DBUF, r1	//		U0DBUF = c;
//1005$:	jnb	_UTX0IF, 1005$
// clr	_UTX0IF
					//	l = ((8+HDR_SIZE+5) & 0x0f )==0 ? (8+HDR_SIZE+5): ((8+HDR_SIZE+5)&0xf0) + 0x10;
		clr	a			// while (i < l) {
0026$:						//		i++;
			movx	@dptr, a//		*p++ = 0;
			inc	dptr	//	}
			djnz	r2, 0026$
0025$:
		mov	_DPL0, r6	//	p2 = (u8 __xdata*)pkt;
		mov	_DPH0, r7
		mov	a, r5
		mov	r3, a
		add	a, r1
		mov	r1, a	// i += len

0028$:			dec	_DPS// #1	//	while (i < l) {
			movx	a, @dptr//		*p++ = *p2++;
			inc	dptr
			inc	_DPS// #0
			movx	@dptr, a
			inc	dptr
			
						//		
			djnz	r3, 0028$//	}
0027$:
//1103$:	jb	_tx_busy, 1103$
// mov	_U0DBUF, #'B'	//		U0DBUF = c;
//1104$:	jnb	_UTX0IF, 1104$
// clr	_UTX0IF
// mov	_U0DBUF, r1	//		U0DBUF = c;
//1105$:	jnb	_UTX0IF, 1105$
// clr	_UTX0IF
		mov	a, r1		// round it up to a 16 byte boundary (for below)
		anl	a, #0xf
		jz	0127$
		mov	r2, a
		mov	a, #16
		clr	c
		subb	a, r2
		mov	r2, a
		clr	a
0128$:
			movx	@dptr, a
			inc	dptr
			djnz	r3, 0128$

0127$:
		// tmp[] contains
		//	0-17	nonce/etc
		//	18-? 	packet header
		//	?->?	rounded to 16 byte boundary
		//	?-?	packet payload
		// r1-> this offset
		//	0-0	up to 16 byte boundary
		dec     _DPS
		mov	dptr, #_iv	//	memset(iv, 0, 16);
		mov	r2, #16
		clr	a
0031$:			movx	@dptr, a
			inc	dptr
			djnz	r2, 0031$

		mov	a, _ENCCS	//	ENCCS &= ~0x70;
		anl	a, #~0x70
		mov	_ENCCS, a
		orl	a, #AES_MODE_CBCMAC	//ENCCS |= AES_MODE_CBCMAC;
		mov	_ENCCS, a


		push	ar5		// len
		push	ar1		// i
		mov	dptr, #_tmp		//	aes_op(tmp, AES_ENCRYPT, i, cipher, iv);
		mov	r0, #_aes_op_PARM_2
		mov	a, #AES_ENCRYPT
		movx	@r0, a
		mov	r0, #_aes_op_PARM_3
		mov	a, r1
		movx	@r0, a
		mov	r0, #_aes_op_PARM_4
		mov	a, #_cipher
		movx	@r0, a
		inc	r0
		mov	a, #_cipher>>8
		movx	@r0, a
		mov	r0, #_aes_op_PARM_5
		mov	a, #_iv
		movx	@r0, a
		lcall	_aes_op
		
		mov	r0, #_iv		//	iv[0] = 1;
		mov	a, #1
		movx	@r0, a
		inc	r0
		mov	r2, #14			//	memcpy(&iv[1],&nonce_tx[1],14);
		mov	r1, #_nonce_tx+1
0039$:			movx	a, @r1
			movx	@r0, a
			inc	r0
			inc	r1
			djnz	r2, 0039$
		clr	a			//	iv[15] = 0;
		movx	@r0, a

		mov	dptr, #_tmp		//	memcpy(tmp, cipher, 16);
		inc	_DPS// #1
		mov	dptr, #_cipher
		mov	r1, #16
0032$:			movx	a, @dptr
			inc	dptr
			dec	_DPS// #0
			movx	@dptr, a
			inc	dptr
			inc	_DPS// #1
			djnz	r1, 0032$

		mov	a, _ENCCS	//	ENCCS &= ~0x70;
		anl	a, #~0x70
		mov	_ENCCS, a
		orl	a, #AES_MODE_CTR	//	ENCCS |= AES_MODE_CTR;
		mov	_ENCCS, a

		dec	_DPS// #0
		mov     dptr, #_tmp	//	aes_op(tmp, AES_ENCRYPT, 16, cipher, iv);
					// aes_op_PARM_2 already set
		mov	r0, #_aes_op_PARM_3
		mov	a, #16
		movx	@r0, a
		mov	r0, #_aes_op_PARM_4
		mov	a, #_cipher
		movx	@r0, a
		inc	r0
		mov	a, #_cipher>>8
		movx	@r0, a
		mov	r0, #_aes_op_PARM_5
		mov	a, #_iv
		movx	@r0, a
		lcall	_aes_op

		pop	ar1	// i

		// note we 0'd it to a 16 byte boundary above

		mov	r0, #_iv+15	//	iv[15] = 1;
		mov	a, #1
		movx	@r0, a

		mov	a, _ENCCS	//	ENCCS &= ~0x70;
		anl	a, #~0x70
		mov	_ENCCS, a
		orl	a, #AES_MODE_CTR	//	ENCCS |= AES_MODE_CTR;
		mov	_ENCCS, a

		pop	ar5	// len
		push	ar5
		clr	c
		mov	a, r1
		subb	a, r5	// i-len
//1203$:	jb	_tx_busy, 1203$
// mov	_U0DBUF, #'C'	//		U0DBUF = c;
//1204$:	jnb	_UTX0IF, 1204$
// clr	_UTX0IF
// mov	_U0DBUF, a	//		U0DBUF = c;
//1205$:	jnb	_UTX0IF, 1205$
// clr	_UTX0IF
// mov	_U0DBUF, r5	//		U0DBUF = c;
//1206$:	jnb	_UTX0IF, 1206$
// clr	_UTX0IF
		add	a, #_tmp 		// aes_op(&tmp[i-len], AES_ENCRYPT, len, &cipher[16], iv);
		mov	dpl, a
		clr 	a
		addc	a, #_tmp>>8
		mov	dph, a			// aes_op_PARM_2 already set
		mov	r0, #_aes_op_PARM_3
		mov	a, r5
		movx	@r0, a			
		mov	r0, #_aes_op_PARM_4
		mov	a, #_cipher+16
		movx	@r0, a
		inc	r0
		mov	a, #(_cipher+16)>>8
		movx	@r0, a
		mov	r0, #_aes_op_PARM_5
		mov	a, #_iv
		movx    @r0, a
		lcall	_aes_op

		mov	r0, #_nonce_tx+9//	nonce_tx[9-12]++
		mov 	r1, #4
0834$:
			movx	a, @r0
			inc	a
			mov	@r0, a
			jnz	0835$
			inc	r0
			djnz	r1, 0834$
		
0835$:
		pop	ar5	// len
		mov	dptr, #_cipher+16		//	p = &cipher[16];
0034$:			movx	a, @dptr	//	while (len--)
			mov	_RFD, a		//		RFD = *p++;
			inc	dptr
			djnz	r5, 0034$

		mov	r5, #LM		//		len = LM;
		mov	dptr, #_cipher	//		p = &cipher[0];	
		sjmp	0005$		//	} else {
0006$:		mov	dpl, r6	//			p = (u8 __xdata*)pkt;
		mov	dph, r7
0005$:					//	}
	// fill buffer
0004$:	
			movx	a, @dptr	// 	while (len--)
			mov	_RFD, a		//		RFD = *p++;
			inc	dptr
			djnz	r5, 0004$
	mov	dptr, #_RFIRQM0		//	RFIRQM0 |= (1<<6);	// enable rx int	RXPKTDONE
	movx	a, @dptr
	orl	a, #1<<6
	movx	@dptr, a

	orl	_IEN2, #1<<0		//	IEN2 = (1<<0);		// RFIE

	mov	_RFST, #0xe9		//	RFST = 0xE9;
0003$:		mov	a, _RFIRQF1	// 	 while(!(RFIRQF1 & (1<<1)) ) // IRQ_TXDONE
		jnb	ACC.1, 0003$	//		;
	mov	_RFIRQF1, #~(1<<1)	//	RFIRQF1 = (1<<1); // IRQ_TXDONE
	ret
	__endasm;
}
