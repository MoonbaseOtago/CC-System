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
static __pdata unsigned int suota_offset;
static __pdata unsigned char suota_version[2];
static __xdata unsigned char suota_remote_mac[8];
static __pdata unsigned long suota_crc;
static __pdata unsigned int suota_addr;

extern __code u8 suota_key[16];

static void suota_timeout(task __xdata * t);

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

static u8
crc_check(code_hdr __code* cp) __naked
{
	__asm;
	mov	r4, #0xff	// msb
	mov	r5, #0xff
	mov	r6, #0xff
	mov	r7, #0xff	// lsb
	push	dpl		// points to CRC
	mov	a, dpl 
	mov	r3, dph 
	add	a, #4		// points to data to CRC
	mov	r0, a
	mov	dpl, a
	clr	a
	movc	a, @a+dptr
	mov	r1, a		// lsb of len	
	inc	dptr
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
				xrl	ar4, #0xb7
crc_f:
			djnz	r0, crc_bits
		djnz r1, crc_1oop
		djnz r2, crc_1oop
	pop	dpl		// compare CRCs
	mov	dph, r3
	clr	a
	movc 	a, @a+dptr
	inc	dptr
	xrl	a, r7
	jnz	crc_fail

	movc 	a, @a+dptr
	inc	dptr
	xrl	a, r6
	jnz	crc_fail

	movc 	a, @a+dptr
	inc	dptr
	xrl	a, r5
	jnz	crc_fail

	movc 	a, @a+dptr
	xrl	a, r4
	jnz	crc_fail
		clr dpl
		ret;
crc_fail:ret	// by def dpl is non 0 here
	__endasm;
}

void
suota_setup()
{
	suota_enabled = 0;
	suota_key_required = 0;	// will be set at APP_INIT
	current_code = &CODE_HEADER;
	if (*((unsigned int __code *)&current_code->len) == 1) {
wait_for_load:
		x_app = (unsigned int (*) (u8 v))&current_code->data[0];
#ifdef SUOTA_CHANNEL
		rf_set_channel(SUOTA_CHANNEL);
#else
		rf_set_channel(11);
#endif
		uart_init();		// remove this
		suota_enabled = 1;	// remove this
		return;
	}
	current_code = (code_hdr __code*)BASE0;
	next_code = (code_hdr __code*)BASE1;
	if (next_code->arch == THIS_ARCH && next_code->code_base == THIS_CODE_BASE &&
	    (current_code->version[1] < next_code->version[1] ||
	    	(current_code->version[1] == next_code->version[1] &&
	       	 current_code->version[0] < next_code->version[0]))) {
		current_code = (code_hdr __code*)BASE1;
		next_code = (code_hdr __code*)BASE0;
	} else
	if (current_code->arch != THIS_ARCH || current_code->code_base != THIS_CODE_BASE) {
		current_code = &CODE_HEADER;
		next_code = (code_hdr __code*)BASE1;
		suota_enabled = 1;
		goto wait_for_load;
	}
	if (!crc_check(current_code)) {
		if (!crc_check(next_code))  {
			current_code = &CODE_HEADER;
			next_code = (code_hdr __code*)BASE1;
			suota_enabled = 1;
			goto wait_for_load;
		}
		{
			__code code_hdr * tmp;
			tmp = current_code;
			current_code = next_code;
			next_code = tmp;
		}
	}
	x_app = (unsigned int (*) (u8 v))&current_code->data[0];
	(*x_app)(APP_LOW_LEVEL_INIT);
}

extern void do_erase_page(unsigned char);
void
flash_erase_page() __naked
{
	__asm;
	.globl	_ep
_ep:
	clr	_EA 
0001$:		movx     a, @dptr	//	while (FCTL & 0x80); //  FCTL.BUSY
		jb	_ACC_7, 0001$
	mov	dptr, #_FADDRH		//	FADDRH = addr>>10;
	pop	acc
	movx	@dptr, a
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
	push	dpl
	mov	dptr, #_tmp_packet
	mov	r0, #_ep_end-_ep
	inc	_DPS
	mov	dptr, #_ep
0001$:		clr	a
		movc	a, @a+dptr
		dec	_DPS
		movx	@dptr, a
		inc	_DPS
		djnz	r0, 0001$
	dec	_DPS
	mov	a, #(_tmp_packet>>8)
	orl	a, #0x80
	mov	dph, a
	mov	dpl, #_tmp_packet
	mov	_MEMCTR, #0x4
	clr	a
	jmp	@a+dptr
	__endasm;
}

extern void do_write_flash(unsigned int);
void
flash_page() __naked
{
	__asm;
	.globl	_fp
_fp:
	clr	_EA 
0001$:		movx     a, @dptr	//	while (FCTL & 0x80); //  FCTL.BUSY
		jb	_ACC_7, 0001$
	mov	dptr, #_FADDRL		//	FADDRL = addr>>2;
	pop	acc
	movx	@dptr, a
	inc	dptr
	pop	acc
	movx	@dptr, a		//	FADDRH = addr>>10;

	mov	dptr, #_FCTL
	movx    a, @dptr
0003$:
		orl	a, #2		//	FCTL |= 0x02; 	     // set FCTL.ERASE 
		movx	@dptr, a
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
0002$:			movx     a, @dptr	//	while (FCTL & 0x80); //  FCTL.BUSY
			jb	_ACC_7, 0002$
		djnz	r0, 0003$
	setb	_EA
	pop	ar2
	pop	ar1
	pop	ar0
	pop	_DPL1
	pop	_DPH1

	mov	r3, #2
0007$:
		clr	c
		mov	a, r1
		rlc	a
		mov	r1, a
		mov	a, r2
		rlc	a
		mov	r2, a
		djnz	r3, 0007$
	mov	a, r0
	add	a, acc
	add	a, acc
	mov	r0, a

0005$:					// check result
		movx	a, @dptr
		inc	dptr
		inc	_DPS
		mov	r1, a
		movx	a, @dptr
		inc	dptr
		dec	_DPS
		xrl	a, r1
		jnz	0006$
		djnz	r0, 0005$
0006$:	mov	dpl, a
	ret
	.globl	_fp_end
_fp_end:
	.globl _do_write_flash
_do_write_flash:
	mov	r0, #_suota_addr
	movx	a, @r0
	mov	_DPL1, a
	push	acc
	inc	r0
	movx	a, @r0
	mov	_DPH1, a
	push	acc

	mov	r0, #_suota_tcount
	movx	a, @r0
	mov	r0, a
	push	acc

	push	dpl
	push	dph
	push	dpl
	push	dph
	mov	dptr, #_tmp_packet
	mov	r0, #_fp_end-_fp
	inc	_DPS
	mov	dptr, #_fp
0001$:		clr	a
		movc	a, @a+dptr
		dec	_DPS
		movx	@dptr, a
		inc	_DPS
		djnz	r0, 0001$
	dec	_DPS


	mov	a, #(_tmp_packet>>8)
	orl	a, #0x80
	mov	dph, a
	mov	dpl, #_tmp_packet
	mov	_MEMCTR, #0x4
	clr	a
	jmp	@a+dptr
	__endasm;
}

void
incoming_suota_packet()
{
	//putstr("incoming_suota_packet\n");
	//puthex(rx_packet->arch);putstr(" "); puthex(rx_packet->code_base);putstr(" "); puthex(current_code->code_base);putstr("\n");
	if (rx_packet->arch!=THIS_ARCH || rx_packet->code_base != current_code->code_base) 
		return;
	if (rx_packet->type == P_TYPE_SUOTA_REQ) {
		unsigned int offset;
		rq = (suota_req __xdata*)&rx_packet->data[0];
		rp = (suota_resp __xdata*)&rx_packet->data[0];
`
		if (rq->version[0] != current_code->version[0] ||
		    rq->version[1] != current_code->version[1])
			return;
		rx_packet->type = P_TYPE_SUOTA_RESP;
		offset = *(unsigned int __xdata *)&rq->offset[0];
		{
			unsigned int l;
			*(unsigned int __xdata *)&rp->total_len[0] = l = (*(unsigned int __xdata *)&current_code->len)+4;
			if (l > offset) 
				memcpy(&rp->data[0], &((u8 __code **)current_code)[offset], sizeof(rp->data));
		}
		rf_send(rx_packet, sizeof(packet)-1+sizeof(suota_resp), SUOTA_KEY, rx_mac);
	} else
	if (rx_packet->type == P_TYPE_SUOTA_RESP) {
		unsigned int len;
		unsigned int offset;

		rq = (suota_req __xdata *)&rx_packet->data[0];
		rp = (suota_resp __xdata *)&rx_packet->data[0];
		offset = *(unsigned int __xdata *)&rq->offset[0];
		if (!suota_state ||
		    rp->version[0] != suota_version[0] ||
		    rp->version[1] != suota_version[1] ||
		    offset != suota_offset) 
			return;
		if (offset >= (BASE1-BASE0))	// don't erase next page
			return;
		cancel_task(&suota_task);
		rx_packet->type = P_TYPE_SUOTA_REQ; 
		len = (*(unsigned int __xdata *)&rp->total_len[0])-offset;
		if (len > 0) {
			unsigned int addr = (unsigned int)next_code;
			unsigned char __xdata* pp;
			unsigned char i;

			if (len > sizeof(rp->data))
				len = sizeof(rp->data);
			
			putstr("flash write 0x");puthex(len);putstr(" bytes to 0x");puthex((addr+offset)>>8); puthex(addr+offset);putstr("\n");
#ifdef NOTDEF
			if (offset == 0) {	// skip over CRC/version we'll do it later - helps us recover from evil
				suota_addr = (unsigned int)&rx_packet->data[8];
				memcpy(&suota_crc, &rx_packet->data[0], 4); 
				suota_tcount = (64-8)>>2;
				addr += 8;
				goto erase_page;
			} else {
				suota_addr = (unsigned int)&rx_packet->data[0];
				addr += offset;
				suota_tcount = 64>>2;
				if (!(addr&0x3ff)) { // 1k boundary?
erase_page:
					do_erase_page(addr>>10);
				}
			}
			if (do_write_flash(addr>>2)) 
				goto do_retry;
#endif
		}
		suota_offset = offset+sizeof(rp->data);
		if (suota_offset >= *(unsigned int __xdata *)&rp->total_len[0]) {
			putstr("preparing to restart\n");
#ifdef NOTDEF
			u8 v;

			//check crc
			EA=0;
			v = next_code->version[0];
			v ^= current_code->version[0];
			if (!(v&1)) { // must copy
				//copy if required
				//write CRC/version, check
				// restart!
			} else {
				//write CRC/version, check
				// restart!
			}
#endif
			return;
		}
		suota_tcount = 5;
		suota_request();
	} 
	
}

static void suota_request()
{
	packet __xdata* pkt = (packet __xdata *)&tmp_packet[0];
	rq = (suota_req __xdata *)&pkt->data[0];
	rq->version[0] = suota_version[0];
	rq->version[1] = suota_version[1];
	rq->arch = THIS_ARCH;
	rq->code_base = THIS_CODE_BASE;
        *(unsigned int __xdata *)&rq->offset[0] = suota_offset;
	pkt->type = P_TYPE_SUOTA_REQ;
	rf_send(pkt, sizeof(packet)-1+sizeof(suota_req), SUOTA_KEY, &suota_remote_mac[0]);
	queue_task(&suota_task, HZ/2);
}

u8
incoming_suota_version(void) 
{
	//putstr("incoming_suota_version type=0x");puthex(rx_packet->type);putstr("\n");
	//puthex(rx_packet->arch);putstr(" "); puthex(rx_packet->code_base);putstr(" "); puthex(current_code->code_base);putstr("\n");
	if (rx_packet->arch != THIS_ARCH || rx_packet->code_base != current_code->code_base) 
		return 0;
	if (rx_packet->version[1] < current_code->version[1] ||
	    (rx_packet->version[1] == current_code->version[1] &&
	     (rx_packet->version[0] <= current_code->version[0])))
		return 0;
	if (rx_packet->version[1] < suota_version[1] ||
	    (rx_packet->version[1] == suota_version[1] &&
	     (rx_packet->version[0] <= suota_version[0])))
		return 0;
	cancel_task(&suota_task);
	suota_version[0] = rx_packet->version[0];
	suota_version[1] = rx_packet->version[1];
	{
		u8 i;
		for (i = 0; i < 8; i++)
			 suota_remote_mac[i] = rx_mac[i];
	}
	suota_offset = 0;
	if (current_code->version[0]&1) {
		suota_state = SUOTA_STATE_UPDATING_1;
		next_code = (code_hdr __code*)BASE1;
	} else {
		suota_state = SUOTA_STATE_UPDATING_0;
		next_code = (code_hdr __code*)BASE0;
	}
	suota_tcount = 5;
	suota_request();
	return 1;
}

static void
suota_timeout(task __xdata*t)
{
	if (suota_tcount == 0) {
		suota_state = SUOTA_STATE_STARTING;
		suota_version[0] = 0;
		suota_version[1] = 0;
		return;
	}
	suota_tcount--;
	if (suota_state == SUOTA_STATE_UPDATING_1 || suota_state == SUOTA_STATE_UPDATING_0) 
		suota_request();
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
