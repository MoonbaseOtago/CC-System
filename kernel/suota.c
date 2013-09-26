
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
//#define SDEBUG

#include <mcs51reg.h>
#include <cc2530.h>
#include "task.h"
#include "interface.h"
#include "suota.h"
#include "rf.h"
#include <string.h>


#define SUOTA_STATE_STARTING	0
#define SUOTA_STATE_RUNNING_0	1
#define SUOTA_STATE_RUNNING_1	2
#define SUOTA_STATE_UPDATING_0	3
#define SUOTA_STATE_UPDATING_1	4
static __pdata u8  suota_state;
static __pdata u8  suota_tcount;
static __pdata u8  suota_retry;
static __pdata u8  suota_code_base;
static __pdata unsigned int suota_offset;
static __pdata unsigned char suota_version[2];
static __xdata unsigned char suota_remote_mac[8];
static __pdata unsigned char suota_crc[4];
static __pdata unsigned int suota_addr;

extern __code u8 suota_key[16];

static void suota_timeout(task __xdata * t);
static void suota_request();
static unsigned char final_fixup();

__xdata task suota_task = {suota_timeout,0,0,0};

extern code_hdr __code CODE_HEADER;

__code code_hdr *__xdata current_code;
__code code_hdr * __xdata next_code;
static suota_req *__xdata rq;
static suota_resp *__xdata rp;

#ifndef BASE0
#define BASE0 (8*1024)			// we assume the 5lSBs  of these are 0
#endif
#ifndef BASE1
#define BASE1 (BASE0+12*1024)
#endif

static void
copy_crc(unsigned char __code *p) __naked
{
	__asm;
	mov	r0, #_suota_crc
	mov	r1, #4
0001$:
		clr	a
		movc	a, @a+dptr
		movx	@r0, a
		inc 	dptr
		inc	r0
		djnz	r1, 0001$
	ret
	__endasm;
}

static void
copy_crc_d(unsigned char __xdata *p) __naked
{
	__asm;
	mov	r0, #_suota_crc
	mov	r1, #4
0001$:
		movx	a, @dptr
		movx	@r0, a
		inc 	dptr
		inc	r0
		djnz	r1, 0001$
	ret
	__endasm;
}

static u8
crc_check(unsigned char __code* cp) __naked
{
	__asm;
	mov	r4, #0xff	// msb
	mov	r5, #0xff
	mov	r6, #0xff
	mov	r7, #0xff	// lsb
	mov	r3, dph 
	mov	r0, dpl
	clr	a
	movc	a, @a+dptr
	mov	r1, a		// lsb of len	
	inc	dptr
	clr	a
	movc	a, @a+dptr
	mov	r2, a		// msb of len
	inc	r2
	mov	dpl, r0		// points to len
crc_1oop:
		mov	r0, #8
		clr	a
		movc	a, @a+dptr
		inc 	dptr
		xrl	a, r4
		mov	r4, a
crc_bits:
			mov	b, r4
			clr	c
			mov	a, r7
			rlc	a
			mov	r7, a
			mov	a, r6
			rlc	a
			mov	r6, a
			mov	a, r5
			rlc	a
			mov	r5, a
			mov	a, r4
			rlc	a
			mov	r4, a
			jnb	b.7, crc_f
				xrl	ar4, #0x04
				xrl	ar5, #0xc1
				xrl	ar6, #0x1d
				xrl	ar7, #0xb7
crc_f:
			djnz	r0, crc_bits
		djnz r1, crc_1oop
		djnz r2, crc_1oop
	mov	r0, #_suota_crc
	movx 	a, @r0
	inc	r0
	xrl	a, r7
	jnz	crc_fail

	movx 	a, @r0
	inc	r0
	xrl	a, r6
	jnz	crc_fail

	movx 	a, @r0
	inc	r0
	xrl	a, r5
	jnz	crc_fail

	movx 	a, @r0
	xrl	a, r4
	jnz	crc_fail
		clr a
crc_fail:mov	dpl, a
	ret
	__endasm;
}

void
suota_setup()
{
#ifdef SDEBUG
uart_init();		
putstr("SUOTA_START\n");
#endif
	suota_enabled = 0;
	suota_allow_any_code = 0;
	suota_key_required = 0;	// will be set at APP_INIT
	if (CODE_HEADER.len[0] == 1) {	// integrated app
#ifdef SDEBUG
putstr("T0\n");
#endif
		goto default_code;
	}
	current_code = (code_hdr __code*)BASE0;
	next_code = (code_hdr __code*)BASE1;
#ifdef SDEBUG
putstr("T1");puthex(current_code->version[0]);puthex(next_code->version[0]);putstr("\n");
#endif
	if ((next_code->version[0]&1) == 0) 
		next_code = 0;
	if (current_code->version[0]&1) {
		current_code = next_code;
		next_code = 0;
	} else
	if (next_code &&
	    next_code->arch == THIS_ARCH && next_code->code_base == THIS_CODE_BASE &&
	    (current_code->version[1] < next_code->version[1] ||
	    	(current_code->version[1] == next_code->version[1] &&
	       	 current_code->version[0] < next_code->version[0]))) {
#ifdef SDEBUG
putstr("T2\n");
#endif
		current_code = (code_hdr __code*)BASE1;
		next_code = (code_hdr __code*)BASE0;
	} 
#ifdef SDEBUG
putstr("T");puthex(((int)current_code)>>8);puthex((int)current_code);putstr(" ");puthex(((int)next_code)>>8);puthex((int)next_code);putstr("\n");
#endif
	if (!current_code || current_code->arch != THIS_ARCH || current_code->code_base != THIS_CODE_BASE) {
#ifdef SDEBUG
putstr("T3\n");
#endif
wait_for_load:
		suota_enabled = 1;
#ifdef SUOTA_CHANNEL
		rf_set_channel(SUOTA_CHANNEL);
#else
		rf_set_channel(11);
#endif
default_code:
		current_code = &CODE_HEADER;
		x_app = (unsigned char (*) (u8 v))&current_code->data[0];
		return;
	}
#ifdef SDEBUG
putstr("C");puthex(current_code->crc[0]);puthex(current_code->crc[1]);puthex(current_code->crc[2]);puthex(current_code->crc[3]);putstr("\n");
#endif
	copy_crc(&current_code->crc[0]);
	if (crc_check(&current_code->len[0])) {	// returns non-0 on fail
#ifdef SDEBUG
putstr("T4\n");
#endif
		if (!next_code) 
			goto nfail;
			
		copy_crc(&next_code->crc[0]);
		if (crc_check(&next_code->len[0]))  {
nfail:
#ifdef SDEBUG
putstr("T5\n");
#endif
			goto wait_for_load;
		}
		current_code = next_code;
		
	}
#ifdef SDEBUG
	putstr("T6\n");//puthex(((unsigned int)current_code)>>8);puthex((unsigned int)current_code);putstr("\n");
#endif
	x_app = (unsigned char (*) (u8 v))&current_code->data[0];
	app(APP_LOW_LEVEL_INIT);
}

extern void do_erase_page();
void
flash_erase_page() __naked
{
	__asm;
	.globl	_ep
_ep:

	clr	_EA 
	mov	dptr, #_FCTL
0001$:		movx     a, @dptr	//	while (FCTL & 0x80); //  FCTL.BUSY
		jb	_ACC_7, 0001$
//	mov	dptr, #_FADDRH		//	FADDRH = addr>>10;
//	pop	acc
//	movx	@dptr, a
	mov	dptr, #_FCTL
	movx	a, @dptr		//	FCTL |= 0x01; 	     // set FCTL.ERASE 
	orl	a, #1
	movx	@dptr, a
0002$:		movx     a, @dptr	//	while (FCTL & 0x80); //  FCTL.BUSY
		jb	_ACC_7, 0002$
	setb	_EA
	ret
	.globl	_ep_end
_ep_end:
	.globl _do_erase_page
_do_erase_page:
//	push	dpl
	mov	dptr, #_cipher
	mov	r0, #_ep_end-_ep
	inc	_DPS
	mov	dptr, #_ep
0001$:		clr	a
		movc	a, @a+dptr
		inc	dptr
		dec	_DPS
		movx	@dptr, a
		inc	dptr
		inc	_DPS
		djnz	r0, 0001$
	dec	_DPS

	mov	dptr, #_cipher
	orl	dph, #0x80
	mov	_FMAP, #0x0
	mov	_MEMCTR, #0x8
	clr	a
	jmp	@a+dptr
	__endasm;
}

static void
read_rom(unsigned char __xdata *addr) __naked
{
	__asm;
	mov	r0, #_suota_addr
	movx	a, @r0
	mov	_DPL1, a
	inc	r0
	movx	a, @r0
	mov	_DPH1, a
	mov	r0, #0
	mov	r1, #4
0001$:
		inc	_DPS
		clr	a
		movc	a, @a+dptr
		inc	dptr
		dec	_DPS
		movx	@dptr, a
		inc	dptr
		djnz	r0, 0001$
		djnz	r1, 0001$
	ret
	__endasm;
}


extern u8 do_write_flash(unsigned int);
void
flash_page() __naked
{
	__asm;
	.globl	_fp
_fp:
	mov	r0, #_suota_tcount
	movx	a, @r0
	mov	r0, a
	push	acc			// 4 - count (words)
	clr	_EA 
	mov	_DPS, #0
	mov	dptr, #_FCTL
0001$:		movx     a, @dptr	//	while (FCTL & 0x80); //  FCTL.BUSY
		jb	_ACC_7, 0001$
	mov	a, #2		//	FCTL |= 0x02; 	     // set FCTL.WRITE 
	movx	@dptr, a
0003$:
		mov	r1, #4
		mov	dptr, #_FWDATA
0004$:
			inc	_DPS		// 
			movx	a, @dptr
			inc	dptr
			dec	_DPS
			movx	@dptr, a
			djnz	r1, 0004$
		mov     dptr, #_FCTL
0002$:			movx     a, @dptr	//	while (FCTL & 0x40); //  FCTL.FULL
			jb	_ACC_6, 0002$
		djnz	r0, 0003$

0011$:	movx     a, @dptr	//	while (FCTL & 0x80); //  FCTL.BUSY
		jb	_ACC_7, 0011$

	mov	a, #4		// enable cache
	movx	@dptr, a
	setb	_EA		
	pop	acc	// 4 -  word count
			// r0 <<= 2 - make byte count
	add	a, acc
	add	a, acc
	mov	r0, a
	pop	_DPH1	// 3 -  hi source addr
	pop	_DPL1	// 2 -  lo source addr
	pop	dph	// 1 -  hi word address
	pop	dpl	// 0 - 	lo word address

0005$:					// check result
		clr	a
		movc	a, @a+dptr
		inc	dptr
		inc	_DPS
		mov	r1, a
		movx	a, @dptr
		inc	dptr
		dec	_DPS
		xrl	a, r1
		jnz	0006$
		djnz	r0, 0005$
0006$:	mov	dpl, a		// non zero means err
	ret
	.globl	_fp_end
_fp_end:
	.globl _do_write_flash
_do_write_flash:


	push	dpl			// 0 - low word address
	push	dph			// 1 - high word address
	mov	dptr, #_cipher
	mov	r1, #_fp_end-_fp
	inc	_DPS
	mov	dptr, #_fp
0001$:		clr	a
		movc	a, @a+dptr
		inc	dptr
		dec	_DPS
		movx	@dptr, a
		inc	dptr
		inc	_DPS
		djnz	r1, 0001$
	dec	_DPS

	mov	r0, #_suota_addr
	movx	a, @r0
	mov	_DPL1, a
	push	acc			// 2 low base of source
	inc	r0
	movx	a, @r0
	mov	_DPH1, a
	push	acc			// 3 hi base of source

	mov	dptr, #_cipher
	orl	dph, #0x80
	mov	_FMAP, #0x0
	mov	_MEMCTR, #0x8
	clr	a
	jmp	@a+dptr
	__endasm;
}

void
incoming_suota_packet_req()	__naked
{
	__asm;		//	unsigned int offset;
	mov	dptr, #_current_code
	movx	a, @dptr
	mov	r0, a		// r0/r1 = current_code
	inc	dptr
	movx	a, @dptr
	mov	dph, a
	mov	r1, a
	mov	dpl, r0
	mov	a, #4
	movc	a, @a+dptr
	add	a, #4
	mov	r2, a		// r2/3 len+4
	mov	a, #5
	movc	a, @a+dptr
	addc	a, #0
	mov	r3, a
	mov	a, #7		// r4 code base
	movc	a, @a+dptr
	mov	r4, a
	mov	a, #8		// r5/6 version
	movc	a, @a+dptr
	mov	r5, a
	mov	a, #9		
	movc	a, @a+dptr
	mov	r6, a

	mov	a, _rx_packet	//	rq = (suota_req __xdata*)&rx_packet->data[0];
	add	a, #5		//	rp = (suota_resp __xdata*)&rx_packet->data[0];
	mov	dpl, a
	mov	a, _rx_packet+1
	addc	a, #0
	mov	dph, a
	movx	a, @dptr	//	if (rq->arch!=THIS_ARCH || rq->code_base != current_code->code_base) 
	cjne	a, #THIS_ARCH, 0001$		//	return;
	sjmp	0003$
0001$:		ret
0003$:
	inc	dptr
	movx	a, @dptr
	cjne	a, ar4, 0001$	//	if (rq->code_base != current_code->code_base)) return
	inc	dptr
	movx	a, @dptr
	cjne    a, ar5, 0001$	//	if (rq->version[0] != current_code->version[0] ||
	inc	dptr		//	    rq->version[1] != current_code->version[1])
	movx	a, @dptr	//		return;
	cjne    a, ar6, 0001$
	inc	dptr		//	offset = *(unsigned int __xdata *)&rq->offset[0];
	movx    a, @dptr
	mov	r5, a
	inc	dptr
	movx    a, @dptr
	mov	r6, a
	inc	dptr
				//	{
	mov	a, r2		//		unsigned int l;	r2/r3
	movx	@dptr, a	//		*(unsigned int __xdata *)&rp->total_len[0] = l = (*(unsigned int __xdata *)&current_code->len)+4;
	inc	dptr
	mov	a, r3
	movx	@dptr, a
	inc	dptr
	
	mov	a, r0
	add	a, r5
	mov	_DPL1, a		//	memcpy(&rp->data[0], &((u8 __code **)current_code)[offset], sizeof(rp->data));
	mov	a, r1
	addc	a, r6
	mov	_DPH1, a
	mov	r0, #64
0002$:		inc	_DPS
		clr	a
		movc	a, @a+dptr
		inc	dptr
		dec	_DPS
		movx	@dptr, a
		inc	dptr
		djnz	r0, 0002$
	mov     dpl, _rx_packet		//	rx_packet->type = P_TYPE_SUOTA_RESP;
	mov     dph, _rx_packet+1
	mov     a, #P_TYPE_SUOTA_RESP
	movx    @dptr, a			//	rf_send(rx_packet, sizeof(packet)-1+sizeof(suota_resp), SUOTA_KEY, rx_mac);
	mov     r0, #_rf_send_PARM_2
	mov     a, #5+72	//0x4D
	movx    @r0, a
	mov     r0, #_rf_send_PARM_3
	mov     a, #SUOTA_KEY
	movx    @r0, a
	mov     r0, #_rf_send_PARM_4
	mov     a, _rx_mac
	movx    @r0, a
	inc     r0
	mov     a, _rx_mac+1
	movx    @r0, a
   	ljmp   _rf_send
	__endasm;
}

void
incoming_suota_packet_resp() 
{
	unsigned int len;
	unsigned int offset;

#ifdef SDEBUG
	putstr("incoming_suota_packet\n");
	puthex(rx_packet->arch);putstr(" "); puthex(rx_packet->code_base);putstr(" "); puthex(suota_code_base);putstr("\n");
#endif

	rp = (suota_resp __xdata *)&rx_packet->data[0];
	if (rp->arch!=THIS_ARCH || rp->code_base != suota_code_base) 
		return;
	offset = *(unsigned int __xdata *)&rp->offset[0];
	if (suota_state == SUOTA_STATE_STARTING ||
	    rp->version[0] != suota_version[0] ||
	    rp->version[1] != suota_version[1] ||
	    offset != suota_offset) 
		return;
	if (offset >= (BASE1-BASE0))	// don't erase next page
		return;
	cancel_task(&suota_task);
	len = (*(unsigned int __xdata *)&rp->total_len[0])-offset;
	if (len > 0) {
		unsigned int addr = (unsigned int)next_code;
		unsigned char first = 1;

		if (len > sizeof(rp->data))
			len = sizeof(rp->data);
		
#ifdef SDEBUG
		putstr("flash write 0x");puthex(len);putstr(" bytes to 0x");puthex((addr+offset)>>8); puthex(addr+offset);putstr("\n");
#endif
		if (offset == 0) {	// skip over CRC/version we'll do it later - helps us recover from evil
			suota_addr = (unsigned int)&rp->data[4];
			copy_crc_d((unsigned char __xdata *)&rp->data[0]);	//	memcpy(&suota_crc[0], &rp->data[0], 4); 
			suota_tcount = (64-4)>>2;
			addr += 4;
			goto erase_page;
		} else {
			suota_addr = (unsigned int)&rp->data[0];
			addr += offset;
			if (offset == (BASE1-BASE0-64)) {
				suota_tcount = (64-16)>>2;
			} else {
				suota_tcount = 64>>2;
			}
			if (!(addr&0x3ff)) { // 1k boundary?
erase_page:
#ifdef SDEBUG
				putstr("erasing page ");puthex(addr>>10);putstr("\n");
#endif
				FADDRH=addr>>10;
				do_erase_page();//do_erase_page(addr>>10);
			}
		}
retry:
#ifdef SDEBUG
		putstr("writing ");puthex(suota_tcount<<2);putstr(" bytes to address ");puthex(addr>>8);puthex(addr);putstr("\n");
#endif
		FADDRL=addr>>2;
		FADDRH=addr>>10;
		if (do_write_flash(addr) != 0) {
#ifdef SDEBUG
			putstr("write failed - retry\n");
#endif
			if (first) {
				first = 0;
				goto retry;
			} else
			if (!suota_retry) {
#ifdef SDEBUG
				putstr("too many retries\n");
#endif
				return;
			}
			suota_retry--;
			suota_offset = offset&~0x3ff;	// force an erase page
			goto try_again;
		}
	}
	suota_offset = offset+sizeof(rp->data);
	if (suota_offset >= *(unsigned int __xdata *)&rp->total_len[0]) {
		unsigned char t;
#ifdef SDEBUG
		putstr("preparing to restart\n");
#endif
		if (crc_check(&next_code->len[0]))  {	// true if failed
#ifdef SDEBUG
			putstr("CRC check failed\n");
#endif
			suota_state = SUOTA_STATE_STARTING;
			suota_version[0] = 0;
			suota_version[1] = 0;
			return;
		}
#ifdef SDEBUG
		putstr("CRC check OK!\n");
#endif
		EA=0;
		
		t = app(APP_SUOTA_START);
			
		final_fixup();

#ifdef SDEBUG
		putstr("restarting-");puthex(t);putstr("\n");
#endif
		if (t&4) {	// erase first page of other portion
			FADDRH = (suota_version[0]&1?(BASE0>>10):(BASE1>>10));
			do_erase_page();//do_erase_page(addr>>10);
		}
		t &= 3;
		if (t == 0) {
			__asm;
			ljmp	0
			__endasm;
		}
		x_app = (unsigned char (*) (u8 v))&current_code->data[0];
       		app(APP_LOW_LEVEL_INIT);
		if ((t&3) == 2) {
			app(APP_SUOTA_DONE);
		} else {
			app(APP_INIT);
		}
		putstr("quit\n");
		{
			extern void task_restart();
			task_restart();	// no deposit no return
		}
	}
	suota_retry = 10;
try_again:
	suota_tcount = 5;
	suota_request();
} 

static void suota_request()
{
	__asm;
	mov	dptr, #_tmp_packet		//	packet __xdata* pkt = (packet __xdata *)&tmp_packet[0];
	
	mov	a, #P_TYPE_SUOTA_REQ		//	pkt->type = P_TYPE_SUOTA_REQ;
	movx	@dptr, a
	mov	dptr, #_tmp_packet+5		//	rq = (suota_req __xdata *)&pkt->data[0];
	mov	a, #THIS_ARCH			//	rq->arch = THIS_ARCH;
	movx	@dptr, a
	inc	dptr
	mov	a, #THIS_CODE_BASE		//	rq->code_base = THIS_CODE_BASE;
	movx	@dptr, a
	inc	dptr
	mov	r0, #_suota_version		//	rq->version[0] = suota_version[0];
	movx	a, @r0
	movx	@dptr, a
	inc	dptr
	inc	r0
	movx	a, @r0				//	rq->version[1] = suota_version[1];
	movx	@dptr, a
	inc	dptr
        mov     r0, #_suota_offset		//	*(unsigned int __xdata *)&rq->offset[0] = suota_offset;
	movx	a, @r0
	movx	@dptr, a
	inc	dptr
	inc	r0
	movx	a, @r0				
	movx	@dptr, a
	__endasm;
	rf_send((packet __xdata*)&tmp_packet[0], sizeof(packet)-1+sizeof(suota_req), SUOTA_KEY, &suota_remote_mac[0]);
	queue_task(&suota_task, HZ/2);
}

u8
incoming_suota_version(void) __naked
{
#ifdef SDEBUG
//	putstr("incoming_suota_version type=0x");puthex(rx_packet->type);putstr("\n");
//	puthex(rx_packet->arch);putstr(" "); puthex(rx_packet->code_base);putstr(" "); puthex(current_code->code_base);putstr("\n");
#endif
	__asm;
	mov	dpl, _rx_packet		// if (rx_packet->arch != THIS_ARCH || (!suota_allow_any_code && rx_packet->code_base != current_code->code_base) )
	mov	dph, _rx_packet+1	// return 0;
	inc	dptr
	movx	a, @dptr
	add	a, #-THIS_ARCH
	jz	0001$
0002$:		mov	dpl, #0
		ret
0001$:	inc	dptr
	movx	a, @dptr
	mov	r2, a	// rx_packet->code_base
	inc	dptr
	movx	a, @dptr
	mov	r3, a	// rx_packet->version[0]
	inc	dptr
	movx	a, @dptr
	mov	r4, a	// rx_packet->version[1]

	mov	dptr, #_current_code
	movx	a, @dptr
	add	a, #4
	mov	r0, a
	inc	dptr
	movx	a, @dptr
	addc	a, #0
	mov	dph, a
	mov 	dpl, r0

	clr	a
	movc	a, @a+dptr // current_code->len[0]
	mov	r6, a
	inc	dptr
	clr	a
	movc	a, @a+dptr // current_code->len[1]
	mov	r7, a
	inc	dptr
	inc	dptr
	clr	a
	movc	a, @a+dptr // code_base
	jb	_suota_allow_any_code, 0003$
		cjne a, ar2, 0002$
0003$:	mov	r0, a
	inc     dptr
	clr	a
	movc	a, @a+dptr
	mov	r5, a		// current_code->version[0]
	mov	a, r0
	cjne	a, ar2, 0004$		//	if (rx_packet->code_base == current_code->code_base &&
	
					//	    rx_packet->version[1] < current_code->version[1] ||
					//	    (rx_packet->version[1] == current_code->version[1] &&
		clr	a		//	     (rx_packet->version[0] <= current_code->version[0])))
		inc	dptr		//		return 0;
		movc	a, @a+dptr
		cjne	a, ar4, 0005$
			mov	a, r5		// current_code->version[0]
			cjne    a, ar3, 0006$
				sjmp	0002$
0006$:			jc	0004$
				sjmp	0002$
0005$:		jnc	0002$

0004$:	
	mov	r1, #_suota_code_base	//	if (rx_packet->code_base == suota_code_base &&
	movx	a, @r1
	cjne	a, ar2, 0014$
	mov	r1, #_suota_state	//	    suota_state != SUOTA_STATE_STARTING &&
	movx	a, @r1
	jz	0014$
		mov	r1, #_suota_version//	    rx_packet->version[1] < suota_version[1] ||
		movx	a, @r1		//	    (rx_packet->version[1] == suota_version[1] &&
		mov	r0, a		//	     (rx_packet->version[0] <= suota_version[0])))
		inc	r1		//		return 0;
		movx	a, @r1
		cjne    a, ar4, 0015$
                        mov     a, r0
                        cjne    a, ar3, 0016$
                                ljmp    0002$
0016$:                  jc      0014$
0066$:                          ljmp    0002$
0015$:          jnc     0066$

0014$:
	mov	r1, #_suota_code_base	//	suota_code_base = rx_packet->code_base;
	mov	a, r2
	movx	@r1, a
	mov	r1, #_suota_version	//	suota_version[0] = rx_packet->version[0];
	mov	a,  r3
	movx	@r1, a
	inc	r1			//	suota_version[1] = rx_packet->version[1];
	mov	a, r4
	movx	@r1, a

	mov	dptr, #_suota_remote_mac //	{
					//		u8 i;
	mov	_DPL1, _rx_mac		//		for (i = 0; i < 8; i++)
	mov	_DPH1, _rx_mac+1
	mov	r0, #8			//			 suota_remote_mac[i] = rx_mac[i];
0020$:		inc	_DPS	
		movx	a, @dptr	//	}
		inc	dptr
		dec	_DPS
		movx	@dptr, a
		inc	dptr
		djnz	r0, 0020$

	mov	r1, #_suota_offset	//	suota_offset = 0;
	clr	a
	movx	@r1, a
	inc	r1
	movx	@r1, a

	mov	a, r6			//	if (current_code->len[1] || current_code->len[0] > 1) {// no version	// r6/r7
	anl	a, #0xfe
	orl	a, r7
	jz	0021$		
		mov	a, r5		//		if (current_code->version[0]&1) {	// r5
		jnb	a.0, 0022$
0024$:			mov	a, #SUOTA_STATE_UPDATING_0//	suota_state = SUOTA_STATE_UPDATING_0;
			mov	r2, #BASE0>>8	//		next_code = (code_hdr __code*)BASE0;
			sjmp	0023$	//		} else {
					//			suota_state = SUOTA_STATE_UPDATING_1;
					//		 	next_code = (code_hdr __code*)BASE1;
					//		}
					//	} else {
0021$:
		mov	a, r3		//		if (suota_version[0]&1) {	// r3
		jnb	a.0, 0024$
0022$:			mov	a, #SUOTA_STATE_UPDATING_1//	suota_state = SUOTA_STATE_UPDATING_1;
			mov	r2, #BASE1>>8//			next_code = (code_hdr __code*)BASE1;
					//		} else {
					//			suota_state = SUOTA_STATE_UPDATING_0;
					//			next_code = (code_hdr __code*)BASE0;
					//		}
					//	}
0023$:	mov	r1, #_suota_state
	movx	@r1, a
	mov	dptr, #_next_code
	clr	a
	movx	@dptr, a
	inc	dptr
	mov	a, r2
	movx	@dptr, a

	mov	dptr, #_suota_task	//	cancel_task(&suota_task);
	lcall	_cancel_task

	mov	r0, #_suota_tcount	//	 suota_tcount = 5;
	mov	a, #5
	movx	@r0, a
	mov	r0, #_suota_retry	//	suota_retry = 10;
	mov	a, #10
	movx	@r0, a

	lcall	_suota_request		//	suota_request();
	mov	dpl, #1			//	return 1;
	ret
	__endasm;
}

static void
suota_timeout(task __xdata*t) __naked
{
	__asm;
#ifdef SDEBUG
	mov	dptr, #0004$
	lcall	_putstr			//	putstr("suota_timeout\n");
#endif
	mov	r0, #_suota_tcount
	movx	a, @r0
	jnz	0001$			//	if (suota_tcount == 0) {
		mov	r0, #_suota_state//		suota_state = SUOTA_STATE_STARTING;
		movx	@r0, a
		mov	r0, #_suota_version//		suota_version[0] = 0;
		movx	@r0, a		//		suota_version[1] = 0;
		inc	r0
		movx	@r0, a
0002$:		ret			//		return;
					//	}
0001$:	dec	a			//	suota_tcount--;
	movx	@r0, a
	mov	r0, #_suota_state	//	if (suota_state == SUOTA_STATE_UPDATING_1 || suota_state == SUOTA_STATE_UPDATING_0) 
	movx	a, @r0
	jz	0002$
		ljmp	_suota_request	//		suota_request();
#ifdef SDEBUG
0004$:	.ascii	"suota_timeout"
	.db	10, 0
#endif
	__endasm;
}

void
suota_get_key()
{
	if (suota_key_required) {
		app(APP_GET_SUOTA_KEY);
	} else {
		rf_set_key_c(&suota_key[0]);
	}
}

static unsigned char
final_fixup() __naked
{
	__asm;
	mov	dptr, #_next_code+1		// if (next_code == (code_hdr __code*)BASE0) {
	movx	a, @dptr
	mov	r0, #_suota_version
	cjne	a, #BASE0>>8, 0001$
						//	if (suota_version[0]&1)
		movx	a, @r0			//		goto must_copy;
		jb	a.0, 0002$
0003$:						//dont_copy:
#ifdef SDEBUG
		mov	dptr, #0700$
		lcall _putstr
#endif
		mov	dptr, #_next_code	//	current_code = next_code;
		movx	a, @dptr
		mov	r0, a
		inc	dptr
		movx	a, @dptr
		mov	r1, a
		mov	dptr, #_current_code
		mov	a, r0
		movx	@dptr, a
		inc	dptr
		mov	a, r1
		movx	@dptr, a
		ljmp	0004$			//		;
0001$:						// } else {
						//	extern __xdata unsigned char xseg_end[2048];
					//	if (!(suota_version[0]&1))
		movx	a, @r0			//		goto must_copy;
		jb	a.0, 0003$
0002$:						//must_copy:
		mov	dptr, #_current_code	// 	if (current_code->len[1] > next_code->len[1] ||
		movx	a, @dptr		//	   (current_code->len[1] == next_code->len[1] &&
		add	a, #5			//	   current_code->len[0] > next_code->len[0])) {
		mov	r0, a
		inc	dptr
		movx	a, @dptr
		addc	a, #0
		mov	dph, a
		mov	dpl, r0
		clr	a
		movc	a, @a+dptr
		mov	r7, a

		mov	dptr, #_next_code
		movx	a, @dptr	
		add	a, #5	
		mov	r0, a
		inc	dptr
		movx	a, @dptr
		addc	a, #0
		mov	dph, a
		mov	dpl, r0
		clr	a
		movc	a, @a+dptr
		cjne	a, #0xff, 0025$		// handle uninitialised ROM cases
			sjmp 	0026$
0025$:		cjne	r7, #0xff, 0027$
			sjmp	0006$
0027$:		cjne    a, ar7, 0005$		//	len = *(unsigned int __code *)&current_code->len[0];
0005$:		jc	0006$			//    } else {
0026$:			mov	a, r7		//	len = *(unsigned int __code *)&next_code->len[0];
0006$:						//    }
		add	a, #3
		anl	a, #~0x03
		mov	r7, a
#ifdef SDEBUG
	
		mov	dptr, #0701$		//	putstr("hard case\n");
		lcall _putstr
#endif
		mov	r6, #0			//	offset = 0;
		mov	r2, #0
//	r7 is length in 256 byte chunks - len
//	r6 is the offset in 256 byte chunks - off
//	r5 is first 
//	r4 is retry
//	r3 is code addrs upper 256 bytes - addr
//	r2 is "fixup retry"
0010$:						//	while (offset < len) {
						//		unsigned char first;
						//		unsigned char retry;
						//		unsigned int addr;
#ifdef SDEBUG
			mov	dptr, #0703$	//		putstr("offset=");puthex(offset);putstr("\n");
			lcall _putstr
			mov	dpl, r6
			lcall _puthex
			mov	dptr, #0702$
			lcall _putstr
#endif
			mov	r5, #1		//		first = 1
			mov	r0, #_suota_addr//		suota_addr = (unsigned int)BASE0+offset;
			clr	a
			movx	@r0, a
			inc	r0
			mov	a, r6
			add	a, #BASE0>>8
			movx	@r0, a
			mov	dptr, #_xseg_end//		read_rom(&xseg_end[0]);
			lcall	_read_rom

			mov	r0, #_suota_addr+1//		suota_addr = (unsigned int)BASE1+offset;
			mov	a, r6
			add	a, #BASE1>>8
			movx	@r0, a
			mov	dptr, #_xseg_end+1024//		read_rom(&xseg_end[1024]);
			lcall	_read_rom

			mov	r4, #5		//		retry = 5;
0011$:						//  retry1:
			mov	r0, #_suota_tcount//		suota_tcount = 256>>2;	
			mov	a, #256>>2
			movx	@r0, a

			mov	a, #BASE0>>8	 //		addr = BASE0+offset;
			mov	dptr, #_xseg_end+1024
0012$:						//		for (;;) {
				add	a, r6	
				mov	r3, a
				mov	r0, #_suota_addr//		suota_addr = (unsigned int)&xseg_end[1024+0*256];	
				mov	a, dpl
				movx	@r0, a
				inc	r0
				mov	a, dph
				movx	@r0, a

				mov	dptr, #_FADDRH	//		FADDRH=addr>>10;
				mov	a, r3
				rr	a
				rr	a
				anl	a, #0x3f
				movx	@dptr, a
	
				lcall	_do_erase_page	//		do_erase_page();

				mov	dptr, #_FADDRL	//		FADDRL= (0*256)>>2;
				clr	a
				movx	@dptr, a
		
				mov	dph, r3	//			if (do_write_flash(addr) != 0) {
				mov	dpl, #0
				lcall	_do_write_flash
				mov	a, dpl
				jnz	0013$
					ljmp	0014$

0013$:						//	xerr:
#ifdef SDEBUG
					mov	dptr, #0704$	//		putstr("fail retry=");puthex(retry);putstr("\n");
					lcall _putstr
					mov	dpl, r4
					lcall	_puthex
					mov	dptr, #0702$
					lcall _putstr
#endif
					mov	a, r4		//		if (retry) {
					jz	0016$
						dec	r4	//			retry--;
						sjmp	0011$	//			goto retry1;
0016$:								//		}
					// undo 
#ifdef SDEBUG
					mov	dptr, #0705$	//		putstr("gave up\n");
					lcall _putstr
#endif
					// now try and put everything back
					// write the live part of the current buffer back
					mov	r0, #_suota_version//		r4 = <to base>
					movx	a, @r0		  //		r7 = <from base>
					jb	a.0, 1000$
						mov	r4, #BASE0>>8
						mov	r7, #BASE1>>8
						mov	r5, #_xseg_end>>8
						sjmp	1001$
1000$:						mov	r4, #BASE1>>8
						mov	r7, #BASE0>>8
						mov	r5, #(_xseg_end+1024)>>8
1001$:
					mov	r2, #4		//	retry = 4
1003$:								//	for (;;) {
						mov	a, r4		 //		addr = BASE0+offset;
						add	a, r6	
						mov	r3, a

						mov	r0, #_suota_addr+1//		suota_addr = (unsigned int)&xseg_end[1024+0*256];	
						mov	a, r5
						movx	@r0, a

						mov	dptr, #_FADDRH	//		FADDRH=addr>>10;
						mov	a, r3
						rr	a
						rr	a
						anl	a, #0x3f
						movx	@dptr, a
	
						lcall	_do_erase_page	//		do_erase_page();

						mov	dptr, #_FADDRL	//		FADDRL= (0*256)>>2;
						clr	a
						movx	@dptr, a
		
						mov	dph, r3	//			if (do_write_flash(addr) != 0) {
						mov	dpl, #0
						lcall	_do_write_flash
						mov	a, dpl
						jz	1014$
1013$:							djnz	r2, 1003$	
							ljmp	_reset_apps
1014$:
						mov	a, #0x40
						lcall	2000$
						jnz	1013$

						mov	a, #0x80
						lcall	2000$
						jnz	1013$

						mov	a, #0xc0
						lcall	2000$
						jnz	1013$

						mov	a, r6	//		if (r6 == 0)
						jz	1004$	//			break;
						add	a, #14
						mov	r6, 1	//		r6-=4
						mov	r2, #4	//		retry = 4

						mov	r0, #_suota_addr//	suota_addr = (unsigned int)BASE0+offset;
						clr	a
						movx	@r0, a
						inc	r0
						mov	a, r6
						add	a, r7
						movx	@r0, a
						mov	dptr, #_xseg_end//	read_rom(&xseg_end[0]);
						lcall	_read_rom
						mov	r5, #_xseg_end>>8
						mov	a, r7
						add	a, #14
						mov	r7, a
						sjmp	1003$	//	}
1004$:
					
0015$:					mov	dpl, #0		//	return 0;
					ret
2000$:			//--------------------------------------------------
						mov	dptr, #_FADDRL	//		FADDRL= (1*256)>>2;
						movx	@dptr, a
						mov	r0, #_suota_addr+1//		suota_addr += 256;	
						movx	a, @r0
						inc	a
						movx	@r0, a
						inc	r3		//		addr += 256;
						mov	dph, r3	//			if (do_write_flash(addr) != 0) 
						mov	dpl, #0		//			goto xerr;
						lcall	_do_write_flash
						mov	a, dpl
						ret
			//--------------------------------------------------
							//		}
0014$:

				mov	a, #0x40
				lcall	2000$
				jz	3001$
3013$:					ljmp	0013$

3001$:				mov	a, #0x80
				lcall	2000$
				jnz	3013$

				mov	a, #0xc0
				lcall	2000$
				jnz	3013$

				mov	a, r5		//		if (!first)
				jz	0017$		//			break;

				mov	r5, #0		//		first = 0;
				
				mov	a, #BASE1>>8	//		addr = BASE1+offset;

				mov	dptr, #_xseg_end
				ljmp	0012$		//	}
0017$:
			mov	a, r6			//	offset += 1024;
			add	a, #4
			mov	r6, a
			cjne	a, ar7, 0020$		//}
				mov	dptr, #_current_code+1
				movx	a, @dptr
				mov	r1, a
				sjmp 0004$	//	}
0020$:			ljmp	0010$ 
0004$:	
	mov	r0, #_suota_tcount	//	suota_tcount = 1;
	mov	a, #1
	movx	@r0, a

#ifdef SDEBUG
	mov	dptr, #0706$		//	putstr("writing crc");
	lcall _putstr
#endif
	mov	r0, #_suota_addr	//	suota_addr = (unsigned int)&suota_crc[0];	// write CRC
	mov	a, #_suota_crc
	movx	@r0, a
	inc	r0
	mov	a, #_suota_crc>>8
	movx	@r0, a
	mov	dptr, #_FADDRL		//	FADDRH=((unsigned int)current_code)>>10;
	clr	a			//	FADDRL=0;
	movx	@dptr, a
	inc	dptr
	mov	a, r1
	rr	a	// lower bits are 0
	rr	a	
	movx	@dptr, a

	mov	dph, r1			//	do_write_flash((unsigned int)current_code);
	mov	dpl, #0
	lcall	_do_write_flash
	mov	dpl, #1
	ret
#ifdef SDEBUG
0700$:	.ascii "simple case"
	.db	10, 0
0701$:	.ascii "hard case"
0702$:	.db	10, 0
0703$:	.ascii "offset="
	.db	0
0704$:	.ascii	"fail retry="
	.db	0
0705$:	.ascii	"gave up"
	.db	10, 0
0706$:	.ascii	"writing crc"
	.db	10, 0
#endif
	__endasm;
}

unsigned char __xdata *suota_allocate_save_space(u8 v)	__naked
{
	__asm;
	mov	r0, dpl
	mov	dptr, #(4*1024)-256-1	// end of xdata
	movx	@dptr, a
	clr	c
	mov	a, dpl
	subb	a, r0
	mov	dpl, a
	mov	a, dph
	subb	a, #0
	mov	dph, a
	ret
	__endasm;
}
unsigned char suota_get_save_size()	__naked
{
	__asm;
	mov	dptr, #(4*1024)-256-1	// end of xdata
	movx	a, @dptr
	mov	dpl, a
	ret
	__endasm;
}

unsigned char __xdata *suota_get_save_space()	__naked
{
	__asm;
	mov	dptr, #(4*1024)-256-1	// end of xdata
	movx	a, @dptr
	mov	r0, a
	clr	c
	mov	a, dpl
	subb	a, r0
	mov	dpl, a
	mov	a, dph
	subb	a, #0
	mov	dph, a
	ret
	__endasm;
}
void reset_apps() __naked
{
	__asm;
	clr	EA
	mov	dptr, #_FADDRH
	mov	a, #BASE0>>10
	movx	@dptr, a
	lcall	_do_erase_page
	mov	dptr, #_FADDRH
	mov	a, #BASE1>>10
	movx	@dptr, a
	lcall	_do_erase_page
	ljmp	0
	__endasm;
	
}
