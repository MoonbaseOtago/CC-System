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
__xdata u8 rx_data0[RX_DATA_SIZE];
__xdata u8 rx_data1[RX_DATA_SIZE];
__bit rx_busy0;
__bit rx_busy1;
__bit rx_set;
__bit rx_rset;
__bit rx_active;
static u8 __data rx_len0;
static u8 __data rx_len1;
u8 __data rx_len;
packet __xdata  * __data rx_packet;
u8 __xdata * __data rx_mac;
static __bit rx_raw=0;
__xdata static u8 tmp[128+32];
__xdata u8 cipher[128];
__pdata static u8 nonce_tx[16];
__pdata static u8 nonce_rx[16];
__pdata static u8 iv[16];
__xdata unsigned char tmp_packet[32];
u8 __data rtx_key;
static u8 __pdata seq;
void rf_receive_on(void);
extern void putstr (const char __code*);
extern void puthex (unsigned char );
__xdata __at (0x616A)  u8 IEEE_MAC[8];
static void aes_done();

//#define RDEBUG
#define WDEBUG
#ifdef RDEBUG
#ifdef WDEBUG
unsigned char __pdata pd_PARM_2;
unsigned char __pdata pdd_PARM_2;
#else
extern void ps(const char __code *);
extern void pf(const char __code *);
extern void pd(unsigned char __xdata *, u8);
extern void pdd(unsigned char __pdata *, u8);
extern void ph(u8);
#endif
void x_ps() __naked 
{
	__asm;
	push	_DPL1
	push	_DPH1
	push	ar0
	push	ar1
	push	ar2
	push	ar3
	push	ar4
	push	ar5
	push	ar6
	push	ar7
#ifdef WDEBUG
	lcall	_putstr
#else
	lcall	_ps
#endif
	pop	ar7
	pop	ar6
	pop	ar5
	pop	ar4
	pop	ar3
	pop	ar2
	pop	ar1
	pop	ar0
	pop	_DPH1
	pop	_DPL1
	ret
	__endasm;
}
void x_pf() __naked 
{
	__asm;
	push	_DPL1
	push	_DPH1
	push	ar0
	push	ar1
	push	ar2
	push	ar3
	push	ar4
	push	ar5
	push	ar6
	push	ar7
#ifdef WDEBUG
	lcall	_putstr
#else
	lcall	_pf
#endif
	pop	ar7
	pop	ar6
	pop	ar5
	pop	ar4
	pop	ar3
	pop	ar2
	pop	ar1
	pop	ar0
	pop	_DPH1
	pop	_DPL1
	ret
	__endasm;
}
void x_pd() __naked 
{
	__asm;
	push	_DPL1
	push	_DPH1
	push	ar0
	push	ar1
	push	ar2
	push	ar3
	push	ar4
	push	ar5
	push	ar6
	push	ar7
#ifdef WDEBUG
	mov	r0, #_pd_PARM_2
	movx	a, @r0
	.globl	_x_pdx
_x_pdx:
	mov	r7, a
	push	dpl
	push	dph
	mov	dpl, a
	lcall	_puthex
	mov	dpl, #' '
	lcall	_putchar
	pop	dpl
	mov	r6, dpl
	lcall	_puthex
	pop	dpl
	mov	r5, dpl
	lcall	_puthex
	mov	dpl, #' '
	lcall	_putchar
	mov	dph, r6
1101$:
		mov	dpl, r5
		movx	a, @dptr
		inc	dptr
		mov	r5, dpl
		mov	dpl, a
		lcall	_puthex
		djnz	r7, 1101$
#else
	lcall	_pd
#endif
	pop	ar7
	pop	ar6
	pop	ar5
	pop	ar4
	pop	ar3
	pop	ar2
	pop	ar1
	pop	ar0
	pop	_DPH1
	pop	_DPL1
	ret
	__endasm;
}
void x_pdd() __naked 
{
	__asm;
	push	_DPL1
	push	_DPH1
	push	ar0
	push	ar1
	push	ar2
	push	ar3
	push	ar4
	push	ar5
	push	ar6
	push	ar7
#ifdef WDEBUG
	mov	dph, #0
	mov	r0, #_pdd_PARM_2
	movx	a, @r0
	sjmp 	_x_pdx
#else
	lcall	_pdd
	pop	ar7
	pop	ar6
	pop	ar5
	pop	ar4
	pop	ar3
	pop	ar2
	pop	ar1
	pop	ar0
	pop	_DPH1
	pop	_DPL1
	ret
#endif
	__endasm;
}
void x_ph() __naked 
{
	__asm;
	push	_DPL1
	push	_DPH1
	push	ar0
	push	ar1
	push	ar2
	push	ar3
	push	ar4
	push	ar5
	push	ar6
	push	ar7
#ifdef WDEBUG
	lcall	_puthex
#else
	lcall	_ph
#endif
	pop	ar7
	pop	ar6
	pop	ar5
	pop	ar4
	pop	ar3
	pop	ar2
	pop	ar1
	pop	ar0
	pop	_DPH1
	pop	_DPL1
	ret
	__endasm;
}
#else
#define ps(x)
#define ph(x)
#define pf(x)
#define pd(x, y)
#define pdd(x, y)
#endif
void
rf_set_key(u8 __xdata * key) __naked
{
	__asm;			//	u8 i;
				//	ps("key=");pd(key, 16);pf("\n");
	mov	a, _ENCCS	//	ENCCS = (ENCCS & ~0x07) | 0x04;	// AES_LOAD_KEY
	anl	a, #~0x7
	orl	a, #4
	mov	_ENCCS, a
    	orl	_ENCCS, #1	//	ENCCS |= 0x01;			// start
	mov	r0, #16		//	for (i=0; i<16; i++) 
0002$:		movx	a, @dptr//		ENCDI = *key++;
		inc	dptr
		mov	_ENCDI, a
		djnz	r0, 0002$
0001$:	mov	a, _ENCCS	//	aes_done();
	jnb	_ACC_3, 0001$
   	ret
	__endasm;
}

void
rf_set_key_c(u8 __code * key) __naked
{
	__asm;			//	u8 i;
				//	ps("key=");pd(key, 16);pf("\n");
	mov	a, _ENCCS	//	ENCCS = (ENCCS & ~0x07) | 0x04;	// AES_LOAD_KEY
	anl	a, #~0x7
	orl	a, #4
	mov	_ENCCS, a
    	orl	_ENCCS, #1	//	ENCCS |= 0x01;			// start
	mov	r0, #16		//	for (i=0; i<16; i++) 
0002$:		clr	a
		movc	a, @a+dptr//		ENCDI = *key++;
		inc	dptr
		mov	_ENCDI, a
		djnz	r0, 0002$
0001$:	mov	a, _ENCCS	//	aes_done();
	jnb	_ACC_3, 0001$
   	ret
	__endasm;
}

void
rf_set_promiscuous(u8 on) __naked
{
	__asm;
	mov	a, dpl
	jnz	0001$
		mov	a, #0xc
		sjmp	0002$
0001$:		mov     a, #0xd
0002$:	mov	dptr, #_FRMFILT0	//	FRMFILT0 = 0x0c|(on?0:1); // filtering on
	movx	@dptr, a
	ret
	__endasm;
}

void
rf_set_raw(u8 on) __naked
{
	__asm;
    	mov	a, dpl			//	if (on) rx_raw = 1; else rx_raw = 0;
	jnz	0001$
		clr	_rx_raw
		ret
0001$:	setb	_rx_raw
	ret
	__endasm;
}

void
rf_set_mac(u8 __xdata *m) __naked
{
	__asm;		//	u8 i;
	mov	r0, #6
	mov	_DPH1, #_IEEE_MAC>>8
	mov	_DPL1, #_IEEE_MAC
0001$:		movx	a, @dptr	//	for (i = 0; i < 6; i++)
		inc	dptr		//		IEEE_MAC[i] = m[i];
		inc	_DPS
		movx	@dptr, a
		inc	dptr
		dec	_DPS
		djnz	r0, 0001$
	mov	r0, #_nonce_tx+7	//	nonce_tx[7] = IEEE_MAC[6] = m[6];
	movx    a, @dptr
	inc     dptr
	inc	_DPS
	movx	@dptr, a
	inc	 dptr
	dec	_DPS
	movx	@r0, a
	inc	r0
	
	movx    a, @dptr		//	nonce_tx[8] = IEEE_MAC[7] = m[7];
	inc	_DPS
	movx	@dptr, a
	dec	_DPS
	movx	@r0, a
	ret
	__endasm;
}

static void
set_def_mac() __naked
{
	__asm;
	mov	dptr, #_IEEE_MAC
	inc	_DPS
	mov	dptr, #14		// we slip 4 bytes into empty space in the int vectors at 14
	lcall	0002$
	mov	dptr, #22		// and the other 4 at 22
	lcall	0002$
	dec	_DPS
	ret
0002$:
	mov	r0, #4
0001$:		clr	a
		movc	a, @a+dptr
		inc	dptr
		dec	_DPS
		movx	@dptr, a
		inc	dptr
		inc	_DPS
		djnz	r0, 0001$
	ret
	__endasm;
}

static void
clear_nonces() __naked
{
	__asm;
    	mov	r2, #16
	mov	r0, #_nonce_rx	//	memset(&nonce_rx[0], 0, sizeof(nonce_rx));
    	mov	r1, #_nonce_tx	//	memset(&nonce_tx[0], 0, sizeof(nonce_tx));
	clr	a
0001$:
		movx	@r1, a
		movx	@r0, a
		inc 	r1
		inc 	r0
		djnz	r2, 0001$
	ret
	__endasm;
}

void
rf_init(void)
{
    rx_busy0 = 0;
    rx_busy1 = 0;
    rx_set = 0;
    rx_rset = 0;
    set_def_mac();
    app(APP_GET_MAC);
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

    clear_nonces();
    nonce_rx[0] = 9;
    nonce_rx[13] = 6;
    nonce_tx[0] = 9;
    nonce_tx[13] = 6;
    nonce_tx[7] = IEEE_MAC[6];
    nonce_tx[8] = IEEE_MAC[7];

    rtx_key = 0xff;

    rf_set_channel(11);	// for a start
    rf_receive_on();
}



void
rf_set_channel(u8 channel)	__naked
{
	__asm;
	mov	a, dpl
    	mov	dptr, #_FREQCTRL 	//	FREQCTRL= 11+(channel-11)*5;
	add	a, #-11	// 0<=a<=15
	mov	r0, a
	rl	a	// 2*a
	rl	a	// 4*a
	add	a, r0	// 5*a
	add	a, #11	// 5*a+11
	movx	@dptr, a
	ret
	__endasm;
}

void
rf_set_transmit_power(char power) __naked
{
	//	switch (power) {
	//	case -3:	FSCTRL = 0x55; TXCTRL = 0x69; TXPOWER = 0x9c; break;
	//	case  0:	FSCTRL = 0x55; TXCTRL = 0x69; TXPOWER = 0xbc; break;
	//	case  4:	FSCTRL = 0x55; TXCTRL = 0x69; TXPOWER = 0xec; break;
	//	case 100:	FSCTRL = 0xF5; TXCTRL = 0x74; TXPOWER = 0xfd; break;
	//	}
	__asm;
	mov	r0, #0x55
	mov	r1, #0x69
	mov	a, dpl
	jnz	0001$			// 0
		mov	a, #0xbc
		sjmp	0007$
0001$:	jnb	a.7, 0002$		// -3
		mov	a, #0x9c
		sjmp	0007$
0002$:	jnb	a.6, 0003$		// 4
		mov	a, #0xec
		sjmp	0007$
0003$:					// 100
		mov	a, #0xfd
		mov	r0, #0xf5
		mov	r1, #0x74
0007$:	mov	dptr, #_TXPOWER
	movx	@dptr, a
	mov	dptr, #_FSCTRL
	mov	a, r0
	movx	@dptr, a
	mov	dptr, #_TXCTRL
	mov	a, r1
	movx	@dptr, a
	ret
	__endasm;
}

void
rf_receive_on(void)
{
	RFST = 0xED;	// flush 
	RFST = 0xED;
	RFST = 0xec;
	RFST = 0xe3;
	rx_active = 1;
}

void
rf_receive_off(void)
{
	rx_active = 0;
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

static void aes_done() __naked
{
	__asm;
0001$:	mov	a, _ENCCS
	jnb	_ACC_3, 0001$
	ret
	__endasm;
}

static void
aes_op_ctr() __naked	//u8 __xdata* p, u8 op, u8 len, u8 __xdata *pOut, u8 __pdata*iv) 
{
	// r0 = tmp
	// r1 = encrypt/decrypt
	// r7 = len
	// r6 = tmp
	// dptr = p
	// dptr1 = pout
	__asm;				//	u8 blocks, j, k, mode, b;
					//	ENCCS &= ~0x7;
	
					//	ENCCS |= AES_MODE_CTR;
	mov	_ENCCS, #AES_MODE_CTR|AES_LOAD_IV
					//	mode = ENCCS&~0x7;
					//	ENCCS = mode|AES_LOAD_IV;
#ifdef RDEBUG
				sjmp	0201$
0202$:				.ascii "iv="
				.db 0x00
0203$:				.db 0x0A
				.db 0x00
0205$:				.ascii "data="
				.db 0x00
0206$:				.ascii "out="
				.db 0x00
0204$:				.ascii "CTR len="
				.db 0x00
0210$:				.ascii " "
				.db 0x00
0201$:				push	_DPL1
				push	_DPH1
				push	dpl
				push	dph
				mov	dptr, #0202$		//	ps("iv=");pdd(iv, 16);pf("\n");
				lcall	_x_ps
				mov	dpl, #_iv
				mov	r0, #_pdd_PARM_2
				mov	a, #16
				movx	@r0, a
				lcall	_x_pdd
				mov	dptr, #0203$
				lcall	_x_pf
#endif
	orl	_ENCCS, #1		//	ENCCS |= 0x01;
	mov	r6, #16
	mov	r0, #_iv
0001$:		movx	a, @r0		//	for(j = 0; j < 16; j++)
		mov	_ENCDI, a	//		ENCDI = *iv++;
		inc	r0
		djnz	r6, 0001$
	
	mov	a, r7			//	blocks = (len+15)&0xf0;
	add	a, #15
	swap	a	
	anl	a, #0xf
	mov	r6, a
					//	ENCCS = mode|op;
 	lcall	_aes_done
	mov	_ENCCS, r1
#ifdef RDEBUG
					//	ps("mode1=");ph(mode);ps(" len=");ph(len);pf("\n");
				mov	dpl, _ENCCS
				lcall	_x_ph
				mov	dptr, #0204$
				lcall	_x_ps
				mov	dpl, r7
				lcall	_x_ph
				mov	dptr, #0210$
				lcall	_x_ps
				mov	dpl, r6
				lcall	_x_ph
				mov	dptr, #0203$
				lcall	_x_pf
					//	ps("data=");pd(p, len);pf("\n");
				mov	dptr, #0205$
				lcall	_x_ps
				pop	dph
				pop	dpl
				push	dpl
				push	dph
				mov	r0, #_pd_PARM_2
				mov	a, r7
	add	a, #15
	anl	a, #0xf0
				movx	@r0, a
				lcall	_x_pd
				mov	dptr, #0203$
				lcall	_x_pf
				pop	dph
				pop	dpl
#endif
0003$:					//	for (b = 0; b < blocks; b+=16) {
 		lcall	_aes_done
		orl	_ENCCS, #1	//		ENCCS |= 0x01;
		mov	r1, #4		//		for (j=0; j < 16; j+=4) {
		
0002$:            	movx	a, @dptr//			for (k=0; k < 4; k++) 
               		mov	_ENCDI, a//				ENCDI = (b+j+k) < len?p[b+j+k]:0x00;
			inc	dptr
            		movx	a, @dptr
               		mov	_ENCDI, a
			inc	dptr
            		movx	a, @dptr
               		mov	_ENCDI, a
			inc	dptr
            		movx	a, @dptr
               		mov	_ENCDI, a
			inc	dptr

			inc	_DPS
			mov	a, _ENCDO//			*pOut++ = ENCDO;
			movx	@dptr, a
			inc	dptr
			mov	a, _ENCDO//			*pOut++ = ENCDO;
			movx	@dptr, a
			inc	dptr
			mov	a, _ENCDO//			*pOut++ = ENCDO;
			movx	@dptr, a
			inc	dptr
			mov	a, _ENCDO//			*pOut++ = ENCDO;
			movx	@dptr, a
			inc	dptr
			dec	_DPS
			djnz	r1, 0002$//		}
		djnz	r6, 0003$	//	}
 		lcall	_aes_done
#ifdef RDEBUG				//	ps("out=");pd(pOut-b, b);pf("\n");
				mov	dpl, _ENCCS
				lcall	_x_ph
				mov	dptr, #0206$
				lcall	_x_ps
				pop	dph
				pop	dpl
				mov	r0, #_pd_PARM_2
				mov	a, r7
	add	a, #15
	anl	a, #0xf0
				movx	@r0, a
				lcall	_x_pd
				mov	dptr, #0203$
				lcall	_x_pf
#endif
	ret			
	__endasm;			
}

static void
aes_op_cbc_mac() __naked	//u8 __xdata* p, u8 op, u8 len, u8 __xdata *pOut, u8 __pdata*iv) 
{
	__asm;				//	u8 blocks, j, k, mode, b;
	// r0 = tmp
	// r1 = tmp/decrypt
	// r7 = len
	// r6 = tmp
	// dptr = p
	// dptr1 = pout
					//	ENCCS &= ~0x7;
					//	ENCCS |= AES_MODE_CTR;
	mov	_ENCCS, #AES_MODE_CBCMAC|AES_LOAD_IV
					//	mode = ENCCS&~0x7;
					//	ENCCS = mode|AES_LOAD_IV;
					//	ps("iv=");pdd(iv, 16);pf("\n");
#ifdef RDEBUG
				sjmp	0201$
0202$:				.ascii "iv="
				.db 0x00
0203$:				.db 0x0A
				.db 0x00
0205$:				.ascii "data="
				.db 0x00
0210$:				.ascii " "
				.db 0x00
0206$:				.ascii "out="
				.db 0x00
0204$:				.ascii "CBC_MAC len="
				.db 0x00
0201$:				push	_DPL1
				push	_DPH1
				push	dpl
				push	dph
				mov	dptr, #0202$		//	ps("iv=");pdd(iv, 16);pf("\n");
				lcall	_x_ps
				mov	dpl, #_iv
				mov	r0, #_pdd_PARM_2
				mov	a, #16
				movx	@r0, a
				lcall	_x_pdd
				mov	dptr, #0203$
				lcall	_x_pf
#endif
	orl	_ENCCS, #1		//	ENCCS |= 0x01;
	mov	r6, #16
	mov	r0, #_iv
0001$:		movx	a, @r0		//	for(j = 0; j < 16; j++)
		mov	_ENCDI, a	//		ENCDI = *iv++;
		inc	r0
		djnz	r6, 0001$

	mov	a, r7			//	blocks = (len+15)&0xf0;
	add	a, #15
	swap	a	
	anl	a, #0xf
	mov	r6, a
					//	ENCCS = mode|op;
 	lcall	_aes_done
	mov	_ENCCS, #AES_ENCRYPT|AES_MODE_CBCMAC
#ifdef RDEBUG
					//	ps("mode2=");ph(mode);ps(" len=");ph(len);pf("\n");
				mov	dpl, _ENCCS
				lcall	_x_ph
				mov	dptr, #0204$
				lcall	_x_ps
				mov	dpl, r7
				lcall	_x_ph
				mov	dptr, #0210$
				lcall	_x_ps
				mov	dpl, r6
				lcall	_x_ph
				mov	dptr, #0203$
				lcall	_x_pf
					//	ps("data=");pd(p, len);pf("\n");
				mov	dptr, #0205$
				lcall	_x_ps
				pop	dph
				pop	dpl
				push	dpl
				push	dph
				mov	r0, #_pd_PARM_2
				mov	a, r7
				add	a, #15
				anl	a, #0xf0
				movx	@r0, a
				lcall	_x_pd
				mov	dptr, #0203$
				lcall	_x_pf
				pop	dph
				pop	dpl
#endif
0002$:					//	for (b = 0; b < blocks; b+=16) {
 		lcall	_aes_done
		orl	_ENCCS, #1	//		ENCCS |= 0x01;
		mov	r7, #16		//		for (j=0; j < 16; j++)
0003$:			movx	a, @dptr//            		ENCDI = (b+j) < len?p[b+j]:0x00;
			mov	_ENCDI, a
			inc	dptr
			djnz	r7, 0003$
			
		cjne	r6, #2, 0004$	//		if (b == (blocks-32)) {
					//			ENCCS &= ~0x70;
 			lcall	_aes_done
			//anl	_ENCCS, #~0x70//			ENCCS |= AES_MODE_CBC;
			//orl	_ENCCS, #AES_MODE_CBC
			mov	_ENCCS, #AES_ENCRYPT|AES_MODE_CBC
0005$:			djnz	r6, 0002$//		} else
0004$:		cjne	r6, #1, 0005$	//		if (b == (blocks-16)) {
			mov	r0, #32//			wait_us(1);
0011$:				djnz	r0, 0011$
			mov	r7, #16	//			for (j=0; j < 16; j++)
			inc	_DPS
0006$:				mov	a, _ENCDO//			*pOut++ = ENCDO;
				movx	@dptr, a
				inc	dptr
				djnz	r7, 0006$
			dec	_DPS
#ifdef RDEBUG				//      ps("out=");pd(pOut-16, 16);pf("\n");
				mov	dpl, _ENCCS
				lcall	_x_ph
				mov	dptr, #0206$
				lcall	_x_ps
				pop	dph
				pop	dpl
				mov	r0, #_pd_PARM_2
				mov	a, #16
				movx	@r0, a
				lcall	_x_pd
				mov	dptr, #0203$
				lcall	_x_pf
#endif
			ret		//		}
					//	}
	__endasm;
}

#define	LM  ((1<<((2&3)+1))&~3)
#define HDR_SIZE (2+1+2+2+8) 
static u8 __xdata* __data rx_data;
void
rcv_handler(task __xdata* ppp) __naked
{
	__asm;
	// u8 hdr;
// data points to
//	0:	2 fxf
//	2:	1 seq
//	3:	2 dst pan broadcast
//	5:	2 dst addr broadcast
//	7:	8 src addr 
//	15:	data 
//
// for crypto packets
//	15:	1 '0xe'
//	16:	4 frame counter
//	20:	key number
//	21:	data
//	n-8:	8 crypto
//
	clr	EA
0001$:						//	for(;;) {
	jnb	_rx_rset, 0002$			//		if (rx_rset) {
		jb	_rx_busy1, 0003$	//			if (!rx_busy1)
0006$:			setb	EA
			ret			//				return;
0003$:		mov	dptr, #_rx_data1	//			rx_data = &rx_data1[0];
		mov	_rx_len, _rx_len1	//			rx_len = rx_len1;
		sjmp	0004$			//		} else {
0002$:		jnb	_rx_busy0, 0006$	//			if (!rx_busy0)
						//				return;
		mov	dptr, #_rx_data0	//			rx_data = &rx_data0[0];
		mov	_rx_len, _rx_len0	//			rx_len = rx_len0;
0004$:		setb	EA
		mov	_rx_data, dpl 		//		}
		mov	_rx_data+1, dph

		movx	a, @dptr
		mov	r0, a
		inc	dptr
		movx	a, @dptr
		jnb	_ACC_2, 0007$		//		if (rx_data[1]&(1<<2)) {	// mac dest
			clr	_rx_broadcast	//			rx_broadcast = 0;	
			mov	r2, #(HDR_SIZE+8+6-2)//			hdr = HDR_SIZE-2+8;
						//			rx_mac = &rx_data[5+8];
			mov	a, #5+8-1
			sjmp	0010$		//		} else {
0007$:			mov	r2, #(HDR_SIZE+6)	//			hdr = HDR_SIZE;
			setb	_rx_broadcast	//			rx_broadcast = 1;
						//			rx_mac = &rx_data[5+2];
			mov	a, #5+2-1
0010$:		add     a, dpl
		mov	_rx_mac, a
		clr	a
		addc	a, dph
		mov	_rx_mac+1, a

		mov	a, r0
		jb	_ACC_3, 0011$		//		if (rx_data[0]&(1<<3) && !rx_raw) { 	// encrypted?	
0013$:			ljmp	0012$
0011$:			jb	_rx_raw, 0013$	//			u8 c, i, l;
						//			u8 __xdata* p;
						//			u8 __xdata* p2;
			//
			// len = rx_len - header len -2 length of data + crypto
			// c = rx_len-5-8	// length of data
			// f = 15+6	// length up to start of data
			// m = 2
			mov	a, _rx_mac	//			nonce_rx[7] = rx_mac[6];
			add	a, #6
			mov	dpl, a
			clr	a
			addc	a, _rx_mac+1
			mov	dph, a
			mov	r1, #_nonce_rx+7
			movx	a, @dptr
			movx	@r1, a
			inc	dptr
			inc 	r1
			movx    a, @dptr	//			nonce_rx[8] = rx_mac[7];
			movx	@r1, a		
			inc	r1

			mov	a, r2
			add	a, #-6
			add	a, _rx_data	//			if (rx_data[hdr] != (6|(1<<3)))
			mov	dpl, a
			clr	a
			addc	a, _rx_data+1
			mov	dph, a
			movx	a, @dptr
			add	a, #-(6|(1<<3))
			jz	0014$
				ljmp	0703$	//				goto done;	// $703
0014$:
			inc	dptr
			mov	r7, #4
0114$:
				movx	a, @dptr//			nonce_rx[9] = rx_data[hdr+1];	
				movx	@r1, a	//			nonce_rx[10] = rx_data[hdr+2];
				inc	dptr	//			nonce_rx[11] = rx_data[hdr+3];
				inc 	r1	//			nonce_rx[12] = rx_data[hdr+4];
				djnz	r7, 0114$
			movx	a, @dptr	//			if (rtx_key != rx_data[hdr+5]) {
			cjne	a, _rtx_key, 0015$
				sjmp	0016$
0015$:
				push	ar2
				mov	_rtx_key, a//				rtx_key = rx_data[hdr+5];
				jnb	_ACC_7, 0415$
					lcall	_suota_get_key
					sjmp	0017$
0415$:				mov	dpl, #APP_GET_KEY//			app(APP_GET_KEY);
				lcall	_app
0017$:				pop	ar2
0016$:						//			}
			mov	r0, #_iv	//			iv[0] = 1;
			mov	a, #1
			movx	@r0, a
			inc 	r0
			mov	r1, #_nonce_rx+1//			memcpy(&iv[1], &nonce_rx[1], 14);
			mov	r3, #14
0020$:				movx	a, @r1
				movx	@r0, a
				inc	r0
				inc	r1
				djnz	r3, 0020$
			clr	a		//			iv[15] = 0;
			movx	@r0, a

#ifdef RDEBUG
				sjmp	0201$
0202$:				.ascii "rcv="
				.db 0x00
0203$:				.db 0x0A
				.db 0x00
0201$:				mov	dptr, #0202$		//	ps("iv=");pdd(iv, 16);pf("\n");
				lcall	_x_ps
				mov	dpl, _rx_data
				mov	dph, _rx_data+1
				mov	r0, #_pd_PARM_2
				mov	a, _rx_len
				movx	@r0, a
				lcall	_x_pd
				mov	dptr, #0203$
				lcall	_x_pf
#endif
			mov	a, _rx_len
			add	a, #-8
			add	a, _rx_data		//		aes_op(&rx_data[rx_len-8], AES_DECRYPT, LM, tmp, iv);
			mov	dpl, a
			clr 	a
			addc	a, _rx_data+1
			mov	dph, a			
			mov	r1, #AES_DECRYPT|AES_MODE_CTR
			mov	r7, #LM
			mov	_DPL1, #_tmp
			mov	_DPH1, #(_tmp)>>8
			lcall	_aes_op_ctr

			mov	a, _rx_len		//		memcpy(&rx_data[rx_len-8], tmp, LM);
			add	a, #-8
			add	a, _rx_data
			mov	_DPL1, a
			clr	a
			addc	a, _rx_data+1
			mov	_DPH1, a
			mov	dptr, #_tmp
			mov	r0, #LM
0121$:				movx	a, @dptr
				inc	dptr
				inc	_DPS
				movx	@dptr, a
				inc	dptr
				dec	_DPS
				djnz	r0, 0121$

			mov	a, _rx_len		//		c = rx_len-8-6-hdr;
			add	a, #-8
			clr	c
			subb	a, r2
			mov	r3, a	// r3 is c
			mov	r0, a
			mov	_DPL1, #_cipher		//		memcpy(&cipher[0], &rx_data[hdr+6], c);
			mov	_DPH1, #_cipher>>8
			mov	a, r2
			add	a, _rx_data
			mov	dpl, a
			clr	a
			addc	a, _rx_data+1
			mov	dph, a
0021$:				movx	a, @dptr
				inc	dptr
				inc	_DPS
				movx	@dptr, a
				inc	dptr
				dec	_DPS
				djnz	r0, 0021$

			mov	a, r3			//		l = (c+15)&0xf0;
			add	a, #15
			anl	a, #0xf0
			clr	c
			subb	a, r3
			jz	0023$
			mov	r0, a
			inc	_DPS
			clr 	a
0022$:				movx	@dptr, a	//		for (i=c; i<l; i++)
				inc	dptr		//			*p++ = 0; 
				djnz	r0, 0022$
			dec	_DPS
0023$:
			mov	r0, #_iv+15		//		iv[15]= 1;
			inc	a // #1
			movx	@r0, a
			

			mov	dptr, #_cipher		//		aes_op(&cipher[0], AES_DECRYPT, c, tmp, iv);
			mov	r7, ar3
			mov	_DPL1, #_tmp
			mov	_DPH1, #(_tmp)>>8
			mov	r1, #AES_DECRYPT|AES_MODE_CTR
			lcall	_aes_op_ctr

			mov	a, r2			//		memcpy(&rx_data[hdr+6], &tmp[0], c);
			add	a, _rx_data
			mov	dpl, a
			clr	a
			addc	a, _rx_data+1
			mov	dph, a
			inc	_DPS
			mov	dptr, #_tmp
			mov	r0, ar3
0024$:				movx	a, @dptr
				inc	dptr
				dec	_DPS
				movx	@dptr, a
				inc	dptr
				inc	_DPS
				djnz	r0, 0024$

			dec	_DPS
			mov	dptr, #_tmp
			mov	a, #0x41 | ((LM-2)/2)<<3//		tmp[0] = 0x41 | ((LM-2)/2)<<3;
			movx	@dptr, a
			inc	dptr
			mov	r0, #_nonce_rx+1
			mov	r1, #13			//		memcpy(&tmp[1], &nonce_rx[1], 13);
0025$:				movx	a, @r0
				inc	r0
				movx	@dptr, a
				inc	dptr
				djnz	r1, 0025$
			clr	a			//		tmp[14] = 0;
			movx	@dptr, a
                        inc 	dptr
			mov	a, r3			//		tmp[15] = c;
			movx	@dptr, a
                        inc 	dptr
			clr	a 			//		tmp[16] = 0;
			movx	@dptr, a
                        inc 	dptr
			mov	a, r2			//		tmp[17] = hdr+6;
			movx	@dptr, a
			mov	r0, a
			add	a, #18
			mov	r1, a	// i
			add	a, #15
			anl	a, #0xf0
			mov	r4, a	// l
                        inc 	dptr
			inc	_DPS			//		memcpy(&tmp[18], &rx_data[0], hdr+6);
			mov	_DPL1, _rx_data
			mov	_DPH1, _rx_data+1
0026$:				movx	a, @dptr
				inc	dptr
				dec	_DPS
				movx	@dptr, a
				inc	dptr
				inc	_DPS
				djnz	r0, 0026$
			dec	_DPS
					// r1 		//		i = 18+hdr+6;
					// r4		//		l = (18+hdr+6+15)&0xf0;
					// dptr		//		p = &tmp[i];
			mov	a, r4			//		while (i < l) {
			clr	c
			subb	a, r1
			jz	0027$
			mov	r0, a
			clr	a
0030$:				movx	@dptr, a	//			i++;
				inc	dptr		//			*p++ = 0;
				djnz	r0, 0030$	//		}
0027$:
			mov	a, _rx_data		//		p2 = &rx_data[hdr+6];
			add	a, r2
			mov	_DPL1, a
			mov	_rx_packet, a	// see below
			clr	a
			addc	a, _rx_data+1
			mov	_DPH1, a
			mov	_rx_packet+1, a

			mov	a, r4			//		l += c;
			add	a, r3
			mov	r4, a
	
			mov	r0, ar3			//		while (i < l) {
0031$:
				inc	_DPS		//			*p++ = *p2++;
				movx	a, @dptr	//			i++;
				inc	dptr
				dec	_DPS
				movx	@dptr, a
				inc	dptr
				djnz	r0, 0031$	//		}

			mov	a, r4			//		l = ((i+15)&0xf0) - i;
			add	a, #15
			anl	a, #0xf0
			mov	r1, a
			clr	c
			subb	a, r4			//		while (l) {
			jz	0032$
			mov	r0, a
			clr	a
0033$:				movx	@dptr, a	//			*p++ = 0;
				inc	dptr		//			l--;
				djnz	r0, 0033$	//		}
0032$:
			mov	r0, #_iv		//		memset(iv,0, sizeof(iv));
			mov	r5, #16
			// a is 0
0034$:				movx	@r0, a
				inc	r0
				djnz	r5, 0034$

			mov	dptr, #_tmp		//		aes_op(&tmp[0], AES_ENCRYPT, i, &cipher[0], &iv[0]);
			mov	r7, ar1
			mov	_DPL1, #_cipher
			mov	_DPH1, #(_cipher)>>8
			lcall	_aes_op_cbc_mac

			mov	r1, #LM			//		if (memcmp(&cipher[0], &rx_data[rx_len-8], LM) != 0)	// discard bogus packet
			mov	dptr, #_cipher		//			goto done;	// $703
			mov	a, _rx_len
			add	a, #-8
			add	a, _rx_data
			mov	_DPL1, a
			clr	a
			addc	a, _rx_data+1
			mov	_DPH1, a
0035$:				inc	_DPS
				movx	a, @dptr
				inc	dptr
				mov	r0, a	
				dec	_DPS
				movx	a, @dptr
				xrl	a, r0
				jnz	0703$
				inc	dptr
				djnz	r1, 0035$
			setb	_rx_crypto		//		rx_crypto = 1;
			//		done above	//		rx_packet = (packet __xdata *)&rx_data[hdr+6];
			mov	_rx_len, r3		//		rx_len = c;
			jnb	_suota_enabled, 0701$	//		if (suota_enabled) {
				lcall	_incoming_suota_version//	 if (incoming_suota_version(rx_packet))
				mov	a, dpl		//			return;
				jnz	0703$

				mov	dpl, _rx_packet	//		if  (rx_packet->type == P_TYPE_SUOTA_REQ || rx_packet->type == P_TYPE_SUOTA_RESP) {
				mov	dph, _rx_packet+1//			incoming_suota_packet(rx_packet, rx_len);
				movx	a, @dptr	//				return;
				cjne	a, #P_TYPE_SUOTA_REQ, 0036$//		x	}
					lcall	_incoming_suota_packet_req
					sjmp	0703$
							//		}
0036$:				cjne	a, #P_TYPE_SUOTA_RESP, 0701$
0037$:					lcall	_incoming_suota_packet_resp
					sjmp	0703$
						//	} else {

0012$:			clr	_rx_crypto		//		rx_crypto = 0;
			mov	a, _rx_len		//		rx_len -= hdr
			clr	c
		     	subb	a, r2
			mov	_rx_len, a
			mov	a, _rx_data		//		rx_packet = (packet __xdata *)&rx_data[hdr];
			add	a, r2;
			mov	_rx_packet, a
			clr	a
			add	a, _rx_data+1
			mov	_rx_packet+1, a
							//	}
0701$:
		mov	dpl, #APP_RCV_PACKET		//	app(APP_RCV_PACKET);
		lcall	_app
0703$:		clr	EA				//done:
		jnb	_rx_rset, 0704$			//	if (rx_rset) {
			clr	_rx_busy1		//		rx_busy1 = 0;
			clr	_rx_rset		//		rx_rset = 0;
			ljmp	0001$			//	} else {
0704$:			clr	_rx_busy0		//		rx_busy0 = 0;
			setb	_rx_rset		//		rx_rset = 1;
							//	}
			ljmp	0001$			// }
	__endasm;
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


static __bit xmt_broadcast, xmt_crypto;
void
rf_send(packet __xdata *pkt, u8 len, u8 crypto, __xdata unsigned char *xmac) __naked
{
	__asm;
	mov	r0, #_rf_send_PARM_3
	movx	a, @r0			//	if (crypto != NO_CRYPTO && rtx_key != crypto) {
	jb	_ACC_7, 0219$
	setb	_xmt_crypto
	cjne	a, _rtx_key, 0215$
		sjmp	0216$
0215$:		push	dpl
		push	dph
		mov	_rtx_key, a			//	add(get_key);
		mov	dpl, #APP_GET_KEY		//	rtx_key = crypto;
		lcall	_app
0212$:
		pop	dph
		pop	dpl
		sjmp	0216$

0217$:		push	dpl
		push	dph
		mov	_rtx_key, a
		lcall   _suota_get_key
		sjmp	0212$

0219$:		clr	_xmt_crypto
		jb	_ACC_0, 0216$
		setb	_xmt_crypto
		cjne    a, _rtx_key, 0217$	// }
0216$:
	mov	r6, dpl
	mov	r7, dph		// pkt
	mov	r0, #_rf_send_PARM_2
	movx	a, @r0
	mov	r5, a		// len
	jnb	_suota_enabled, 0700$	//	if (suota_enabled) {
		inc	dptr
		inc	_DPS
		mov	dptr, #_current_code//    	pkt->arch = current_code->arch;
		movx	a, @dptr	//		pkt->code_base = current_code->code_base
		add	a, #6		//              memcpy(&pkt->version[0], &current_code->version[2], 3);
		mov	r1, a
		inc	dptr
		movx	a, @dptr
		mov	_DPL1, r1
		addc	a, #0
		mov	_DPH1, a
		mov	r2, #4
0019$:				       // = #1
			clr	a
			movc	a, @a+dptr
			inc	dptr
			dec	_DPS	// = #0
			movx	@dptr, a
			inc	dptr
			inc	_DPS
			djnz	r2, 0019$
		dec	_DPS
//mov dpl, r6
//mov dph, r7
//mov	r0, #_pd_PARM_2
//mov	a, r5
//movx	@r0, a
//lcall	_x_pd
//sjmp 3333$
//3334$:	.ascii "<-BUFF"
	//.db	10, 0
//3333$:
//mov	dptr, #3334$
//lcall	_x_pf

0700$:	mov	dptr, #_FSMSTAT1//	while (FSMSTAT1 & ((1<<1) | (1<<5)))	// SFD | TX_ACTIVE
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

	//	set key
	//
		
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
	mov	r2, a
	jz	2012$
		clr	_xmt_broadcast
		mov	a, #6
		mov	r2, #1<<2
2012$:
	jb	_xmt_crypto, 0012$
		add	a, r5		//		RFD = len+HDR_SIZE+0;
		add	a, #HDR_SIZE+2
		mov	_RFD, a
		mov	a, #(1<<0)|(1<<6)//	} else {
		sjmp	0013$

0012$:	 	add	a, r5		//		RFD = len+HDR_SIZE+6;
		add	a, #HDR_SIZE+6+8+2
		mov	_RFD, a
		mov     a, #(1<<0)|(1<<6)|(1<<3)//}

0013$:
	mov	dptr, #_tmp+18		//	p = &tmp[18];
	mov	_RFD, a			//	RFD = *p++ =  	(1<<0)|	// DATA			fc0
	movx	@dptr, a		//	   		(crypto?(1<<3):(0<<3))|	// crypto
	inc	dptr			//	      		(0<<4)|	// frame pending
					//	      		(0<<5)|	// ack request 
					//	     		(1<<6);	// pan compression
	mov	a, r2			// addressing type
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
		inc	dptr
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
	
	jb	_xmt_crypto, 9006$
		ljmp	0006$
9006$:
		mov	a, #6|(1<<3)
		mov	_RFD,a	//		RFD = 0x06|(1<<3);
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
		mov	a, _rtx_key
		mov     _RFD, a //      RFD = rtx_key
		movx    @dptr, a
		inc	dptr
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
//	f = HDR_SIZE+6
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
			mov	a, #HDR_SIZE+6+8-2
			movx	@dptr, a	
			mov	dptr, #_tmp+(18+HDR_SIZE+6+8-2);
			mov 	r1, #((18+HDR_SIZE+6+8-2+15)&0xf0) //	i = 18+HDR_SIZE+6;
			mov	r2, #((18+HDR_SIZE+6+8-2+15)&0xf0)-(18+HDR_SIZE+6+8-2)  
			sjmp	5502$
5501$:
			mov	a, #HDR_SIZE+6	//	tmp[17] = HDR_SIZE+6;
			movx	@dptr, a	
			mov	dptr, #_tmp+(18+HDR_SIZE+6);
			mov 	r1, #((18+HDR_SIZE+6+15)&0xf0) //	i = 18+HDR_SIZE+6;
			mov	r2, #((18+HDR_SIZE+6+15)&0xf0)-(18+HDR_SIZE+6)
5502$:
//1003$:	jb	_tx_busy, 1003$
// mov	_U0DBUF, #'A'	//		U0DBUF = c;
//1004$:	jnb	_UTX0IF, 1004$
// clr	_UTX0IF
// mov	_U0DBUF, r1	//		U0DBUF = c;
//1005$:	jnb	_UTX0IF, 1005$
// clr	_UTX0IF
					//	l = ((8+HDR_SIZE+6) & 0x0f )==0 ? (8+HDR_SIZE+6): ((8+HDR_SIZE+6)&0xf0) + 0x10;
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
			djnz	r2, 0128$

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

		push	ar1		// i
		mov	dptr, #_tmp		//	aes_op(tmp, AES_ENCRYPT, i, cipher, iv);
		mov	r7, ar1
		mov	_DPL1, #_cipher
		mov	_DPH1, #_cipher>>8
		lcall	_aes_op_cbc_mac
#ifdef RDEBUG
				sjmp	5201$
5202$:				.ascii "X1"
5203$:				.db 0x0A
				.db 0x00
5201$:				mov	dptr, #5202$		//	ps("iv=");pdd(iv, 16);pf("\n");
				lcall	_x_pf
#endif
		
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

		dec	_DPS// #0
		mov     dptr, #_tmp	//	aes_op(tmp, AES_ENCRYPT, 16, cipher, iv);
					// aes_op_PARM_2 already set
		mov	r7, #16
		mov	_DPL1, #_cipher
		mov	_DPH1, #_cipher>>8
		mov	r1, #AES_ENCRYPT|AES_MODE_CTR
		lcall	_aes_op_ctr

		pop	ar1	// i

		// note we 0'd it to a 16 byte boundary above

		mov	r0, #_iv+15	//	iv[15] = 1;
		mov	a, #1
		movx	@r0, a

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
		mov	r7, ar5
		mov	_DPL1, #_cipher+16
		mov	_DPH1, #(_cipher+16)>>8
		mov	r1, #AES_ENCRYPT|AES_MODE_CTR
		lcall	_aes_op_ctr

		mov	r0, #_nonce_tx+9//	nonce_tx[9-12]++
		mov 	r1, #4
0834$:
			movx	a, @r0
			inc	a
			movx	@r0, a
			jnz	0835$
			inc	r0
			djnz	r1, 0834$
		
0835$:
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
