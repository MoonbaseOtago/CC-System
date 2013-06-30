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

//#define XMT
#include <mcs51reg.h>
#include <cc2530.h>
#include "task.h"
#include "rf.h"
#include "suota.h"
#include "interface.h"

static task __xdata * __data waitq;
static volatile u8 __data enter_sleep_mod_flag;
void putstr(char __code *cp);
#define radio_busy 1

static unsigned int dummy_app(u8 v) { return 0; }
unsigned int (* __data x_app) (u8 v) = dummy_app;

static unsigned int __data last;
static unsigned char __data tmp0;
static unsigned char __data tmp1;

static void
iretx() __naked
{	
	__asm;
	reti
	__endasm;
}
extern void sleep();
//extern void rf_err_isr()  __interrupt(0);
extern void rf_isr()  __interrupt(16);
#ifdef KEYS
extern void t1_isr()  __interrupt(9);
extern void keys_on();
#else
void (* __data t1_vect) () = iretx;
void t1_isr()  __interrupt(9) __naked
{	
	__asm;
	push	_t1_vect
	push	_t1_vect+1
	ret
	__endasm;
}
#endif

void (* __data uart_rx_0_vect) () = iretx;
void (* __data uart_rx_1_vect) () = iretx;
void (* __data uart_tx_0_vect) () = iretx;
void (* __data uart_tx_1_vect) () = iretx;
void (* __data p0_vect) () = iretx;
void (* __data p1_vect) () = iretx;
void (* __data p2_vect) () = iretx;
void (* __data t2_vect) () = iretx;
void (* __data t3_vect) () = iretx;
void (* __data t4_vect) () = iretx;
void (* __data adc_vect) () = iretx;
void (* __data aec_vect) () = iretx;
void (* __data dma_vect) () = iretx;
void uart_rx_0_isr()  __interrupt(2) __naked
{	
	__asm;
	push	_uart_rx_0_vect
	push	_uart_rx_0_vect+1
	ret
	__endasm;
}
void uart_rx_1_isr()  __interrupt(3) __naked
{	
	__asm;
	push	_uart_rx_1_vect
	push	_uart_rx_1_vect+1
	ret
	__endasm;
}
void uart_tx_0_isr()  __interrupt(7) __naked
{	
	__asm;
	push	_uart_tx_0_vect
	push	_uart_tx_0_vect+1
	ret
	__endasm;
}
void uart_tx_1_isr()  __interrupt(14) __naked
{	
	__asm;
	push	_uart_tx_1_vect
	push	_uart_tx_1_vect+1
	ret
	__endasm;
}
void p0_isr()  __interrupt(13) __naked
{	
	__asm;
	push	_p0_vect
	push	_p0_vect+1
	ret
	__endasm;
}
void p1_isr()  __interrupt(15) __naked
{	
	__asm;
	push	_p1_vect
	push	_p1_vect+1
	ret
	__endasm;
}
void p2_isr()  __interrupt(6) __naked
{	
	__asm;
	push	_p2_vect
	push	_p2_vect+1
	ret
	__endasm;
}
void t2_isr()  __interrupt(10) __naked
{	
	__asm;
	push	_t2_vect
	push	_t2_vect+1
	ret
	__endasm;
}
void t3_isr()  __interrupt(11) __naked
{	
	__asm;
	push	_t3_vect
	push	_t3_vect+1
	ret
	__endasm;
}
void t4_isr()  __interrupt(12) __naked
{	
	__asm;
	push	_t4_vect
	push	_t4_vect+1
	ret
	__endasm;
}
void adc_isr()  __interrupt(1) __naked
{	
	__asm;
	push	_adc_vect
	push	_adc_vect+1
	ret
	__endasm;
}
void aec_isr()  __interrupt(4) __naked
{	
	__asm;
	push	_aec_vect
	push	_aec_vect+1
	ret
	__endasm;
}
void dma_isr()  __interrupt(8) __naked
{	
	__asm;
	push	_dma_vect
	push	_dma_vect+1
	ret
	__endasm;
}

unsigned char
daylight() __naked
{
	__asm;
	clr	a
	jb	P0.7, 0001$
		inc	a
0001$:
	mov	dpl, a
	ret
	__endasm;
}

static void
xxxx_sleep() __naked
{
	__asm;
	.odd
_sleep:
	mov 	PCON, _enter_sleep_mod_flag
	ret
	__endasm;
}
void
start_timer(unsigned int v) __naked
{
	__asm;
	mov	a, _ST0
	mov	b, a
	mov	a, _ST1
	mov	_last, a
	add	a, dpl
	mov	dpl, a
	mov	a, _ST2
	mov	_last+1, a
	addc	a, dph
	mov	dph, a
xxx:	mov	a, _STLOAD
	jnb	a.0, xxx
	mov	_ST2, dph
	mov	_ST1, dpl
	mov	a, b
	mov	_ST0, a
	setb	_STIE
	ret
	__endasm;
}

unsigned int 
get_timer() __naked
{
	__asm;
	mov	a, _ST0
	mov	dpl, _ST1
	mov	dph, _ST2
	ret
	__endasm;
}

unsigned int
stop_timer() __naked
{
	__asm;
	clr	_STIE
	mov	a, _ST0
	clr	c
	mov	a, _ST1
	subb	a, _last
	mov	dpl, a
	mov	a, _ST2
	subb	a, _last+1
	mov	dph, a
	ret
	__endasm;
}

unsigned int
save_timer() __naked
{
	__asm;
	mov	a, _ST0
	clr	c
	mov	a, _ST1
	mov	r0, a
	subb	a, _last
	mov	dpl, a
	mov	_last, r0
	mov	a, _ST2
	mov	r0, a
	subb	a, _last+1
	mov	dph, a
	mov	_last+1, r0
	ret
	__endasm;
}

void sleep_isr()  __interrupt(5) __naked
{
	__asm;
	clr	_STIE
	mov	_enter_sleep_mod_flag, #0
	reti
	__endasm;
}

void putchar(char c)	__naked`
{
	__asm;
	mov	_U0DBUF, dpl	//		U0DBUF = c;
0004$:	jnb	_UTX0IF, 0004$
	clr	_UTX0IF
	ret
	__endasm;
}
void
putstr(char __code *cp) __naked
{
	__asm;
0001$:			//	for (;;) {
	clr	a
	movc	a, @a+dptr//		// c = *cp++;
	inc	dptr
	jnz	0002$  //		if (!c) break;
		ret
0002$: 	cjne	a, #'\n', 0003$	//	if (*cp == '\n') {
	mov	_U0DBUF, #13	//		U0DBUF = 13;
0004$:	jnb	_UTX0IF, 0004$
	clr	_UTX0IF
	mov	_U0DBUF, #10	//		U0DBUF = 10;
0014$:	jnb	_UTX0IF, 0014$
	clr	_UTX0IF
	sjmp    0001$
0003$:		//		} else {
	mov	_U0DBUF, a	//		U0DBUF = c;
0034$:	jnb	_UTX0IF, 0034$
	clr	_UTX0IF
	sjmp	0001$		//	}
	__endasm;		// }
}
void
puthex(unsigned char v)	__naked
{
	__asm;
	mov	a, dpl
	swap	a
	anl	a, #0xf		//	i = (v>>4)&0xf;
	cjne	a, #9, 0001$	//	if (i <= 9) {
0002$:
		add	a, #'0'	//	 	i += '0'	
		
		sjmp	0003$	//	} else {
0001$:	jc	0002$
		add	a, #'a'-10//		i += 'a'-10
				//	}
0003$:	mov	_U0DBUF, a	//	putchar(i);
0094$:	jnb	_UTX0IF, 0094$
	clr	_UTX0IF

0024$:
	mov	a, dpl
	anl	a, #0xf		//	i = v&0xf;
	cjne	a, #9, 0011$	//	if (i <= 9) {
0012$:
		add	a, #'0'	//	 	i += '0'	
		
		sjmp	0013$	//	} else {
0011$:	jc	0012$
		add	a, #'a'-10//		i += 'a'-10
				//	}
0013$:	mov	_U0DBUF, a	//	putchar(i);
0084$:	jnb	_UTX0IF, 0084$
	clr	_UTX0IF
	ret
	__endasm;
}

void
wait_us(unsigned char t)
{
    while (t--) {
	__asm
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	__endasm;
    }
}

void queue_task_0(task __xdata * t) __naked	// for ISRs - save param so we don't trash global
{
	__asm;
	mov	r0, #_queue_task_PARM_2	
	movx	a, @r0
	push	ACC
	clr	a
	movx	@r0, a
	inc	r0
	movx	a, @r0
	push	ACC
	clr	a
	movx	@r0, a
	acall	_queue_task
	mov	r0, #_queue_task_PARM_2	
	pop	ACC
	mov	@r0, a
	inc	r0
	pop	ACC
	mov	@r0, a
	ret
	__endasm;
}

void
queue_task(task __xdata * t, unsigned int time) __naked
{
	__asm;
	mov	r4, dpl
	mov	r5, dph		// t
	push 	_IEN0
	clr	EA
	acall	_save_timer// delta = stop_timer()
	mov	a, dpl		// delta
	mov	r6, a
	mov	r7, dph	
//	for (pt = waitq; delta && pt; pt = pt->next) {
//		if (pt->time > delta) {
//			pt->time -= delta;
//			break;
//		}
//		delta -= pt->time;
//		pt->time = 0;
//		enter_sleep_mod_flag = 0;
//	}
	orl	a, r7		//		for (;;) {
	jz	qt_2		//			if (!delta) break
	mov	a, _waitq
	mov	dpl, a
	mov	dph, _waitq+1
	orl	a, dph
	jz	qt_2
qt_1:	// a contains dpl
		inc	dptr
		inc	dptr	// points at time
		movx	a, @dptr
		mov	_DPL1, dpl
		mov	_DPH1, dph
		clr	c
		subb	a, r6
		mov	r2, a
		inc	dptr
		movx	a, @dptr
		subb	a, r7
		mov	_DPS, #1
		jc	qt_3		// if (pt->time > delta) 
			mov	r0, a		// pt->time -= delta;
			mov	a, r2
			movx	@dptr, a
			inc 	dptr
			mov	a, r0
			movx	@dptr, a
			clr	_DPS
			sjmp	qt_2
qt_3:		mov	r3, a	
		clr	a	// pt->time = 0;
		movx	@dptr, a	
		inc 	dptr
		movx	@dptr, a
		inc 	dptr

		mov	_enter_sleep_mod_flag, #0 // enter_sleep_mod_flag = 0;
		subb	a, r2	// delta -= pt->time;
		mov	r6, a
		clr	a
		subb	a, r3
		mov	r7, a
		movx	a, @dptr// pt = pt->next
		mov	r0, a
		inc 	dptr
		mov	_DPL0, a
		movx	a, @dptr
		mov	_DPH0, a
		clr	_DPS
		orl	a, r0	// if pt == null break
		jz	qt_2
		mov	a, r6
		orl	a, r7
		jnz	qt_1
qt_2:
//	if (t->state == TASK_QUEUED) {
//		t->state = TASK_IDLE;
//		pt = waitq;
//		ptp = &waitq;
//		for (;;) {
//			if (pt == t) {
//				if ((*ptp = pt = t->next) != 0) 
//					pt->time += t->time;
//				break;
//			}
//			ptp = &pt->next;
//			pt = *ptp;
//		}
//	}

	mov	a, r4	//	 if (t->state == TASK_QUEUED)
	add	a, #6
	mov	dpl, a
	mov	a, r5
	addc	a, #0
	mov	dph, a
	movx	a, @dptr
	cjne	a, #TASK_QUEUED, qt_4
		mov	a, #TASK_IDLE		// t->state = TASK_IDLE;
		movx	@dptr, a
		mov	r2, #0
		mov	r0, _waitq		// for (pt = waitq;;) {
		mov	r1, _waitq+1
qt_5:
			mov	a, r0		// if (pt == t) {
			cjne	a, ar4, qt_6
			mov	a, r1
			cjne	a, ar5, qt_6
				mov	dpl, r4		
				mov	dph, r5
				inc 	dptr
				inc 	dptr
				movx	a, @dptr	// r1:r0 = t->time
				inc 	dptr
				mov	r0, a
				movx	a, @dptr
				inc 	dptr
				mov	r1, a

				movx	a, @dptr	// pt = dptr0 = t->next
				inc 	dptr
				mov	r3, a
				movx	a, @dptr
				mov	dph, a		
				mov	dpl, r3

				cjne	r2, #0, qt_8	// if (prev)
					mov	_enter_sleep_mod_flag, #0 // enter_sleep_mod_flag = 0;
					mov	_waitq, dpl	// 	waitq = dptr0
					mov	_waitq+1, dph
					sjmp	qt_9		// else
qt_8:
					mov 	_DPS, #1
					mov	a, _DPL0 	//	prev->next = dptr0
					movx	@dptr, a
					inc	dptr
					mov	a, _DPH0
					movx	@dptr, a
					mov 	_DPS, #0
qt_9:
				mov	a, dpl
				orl	a, dph
				jz	qt_4
					inc 	dptr	
					inc 	dptr
					movx	a, @dptr	 // pt->time += t->time;
					add	a, r0
					movx	@dptr, a
					inc 	dptr
					movx	a, @dptr
					movx	@dptr, a
					sjmp	qt_4

qt_6:
			mov	r1, #1
			mov	dpl, r0
			mov	dph, r1
			inc	dptr
			inc	dptr
			inc	dptr
			inc	dptr	
			mov	_DPL1, dpl	// prev = pt
			mov	_DPH1, dph
			movx	a, @dptr	// pt = pt->next
			mov	r0, a
			inc	dptr
			movx	a, @dptr	//}
			mov	r1, a
			sjmp	qt_5
qt_4:
//	ptp = &waitq;
//	pt = waitq;
//	for (;;) {
//		while (pt && pt->time == 0) {
//			ptp = &pt->next;
//			pt = *ptp;
//		}
//		if (!pt) {
//			t->time = time;
//			t->next = 0;
//			t->state = TASK_QUEUED;
//			*ptp = t;
//			break;
//		}
//		if (pt->time <= time) {
//			time -= pt->time;
//			ptp = &pt->next;
//			pt = *ptp;
//			continue;
//		}
//		pt->time -= time;
//		t->time = time;
//		t->next = pt;
//		*ptp = t;
//		t->state = TASK_QUEUED;
//		break;
//	}
	mov	dpl, _waitq	// pt = waitq
	mov	dph, _waitq+1
	mov	r2, #0		// prev marked invalid
qt_11:						// for (;;) {
		mov	r6, dpl			// save pt in r7:r6
		mov	r7, dph
		mov	a, r6			// 
		orl	a, r7			//	if (!pt) break;
		jz	qt_12


		inc	dptr			//	if (pt->time != 0) break;
		inc	dptr
		movx	a, @dptr
		mov	r3, a
		inc	dptr
		movx	a, @dptr
		orl	a, r3
		jnz	qt_13

			mov	r2, #1		// mark prev valid
			inc	dptr
			mov	_DPL1, dpl	//	prev = &pt->next
			mov	_DPH1, dph
			movx	a, @dptr	//      pt = pt->next
			mov	r3, a
			inc	dptr
			movx	a, @dptr
			mov	dph, a
			mov	dpl, r3
			sjmp	qt_11		// }
qt_12:						// if (pt == 0) {	// end of Q
		
qt_99:	
		cjne	r2, #0, qt_14		//      if (prev) {
			mov	_enter_sleep_mod_flag, #0 // enter_sleep_mod_flag = 0;
			mov	_waitq, r4	//		waitq = t
			mov	_waitq+1, r5	//	
			sjmp	qt_15
qt_14:						//      } else {
			mov	_DPS, #1
			mov	a, r4
			movx	@dptr, a	//		*prev = t
			inc	dptr
			mov	a, r5
			movx	@dptr, a
			mov	_DPS, #0	//	}
qt_15:		mov	dpl, r4	// dptr = t
		mov	dph, r5
		inc	dptr
		inc	dptr
		mov	r0, #_queue_task_PARM_2	//	t->time = time;
		movx	a, @r0
		movx	@dptr, a
		inc	dptr
		inc	r0
		movx	a, @r0
		movx	@dptr, a
		inc	dptr
		mov	a, r6	
		movx	@dptr, a		//	t->next = next;
		inc	dptr
		mov	a, r7	
		movx	@dptr, a
		inc	dptr
		mov	a, #TASK_QUEUED		//	t->state = TASK_QUEUED;
		movx	@dptr, a
		pop	_IEN0
		ret				//	return
						// }
qt_13:	
		mov	dpl, r6			// 
		mov	dph, r7
		inc	dptr		
		inc	dptr		
		clr	c
		mov	r0, #_queue_task_PARM_2
		movx	a, @r0
		mov	r1, a
		movx	a, @dptr
		subb	a, r1			// tmp1:0 = pt->time - time
		inc	dptr		
		mov	_tmp0, a
		inc	r0
		movx	a, @r0
		mov	r1, a
		movx	a, @dptr
		subb	a, r1
		mov	_tmp1, a
		jc	qt_98			// if pt->time was >= time (must insert t prior to pt)
			mov     dpl, r6
			mov     dph, r7			
			inc	dptr	
			inc	dptr		
			mov	a, _tmp0	//	pt->time = p->time - time
			movx    @dptr, a
			inc	dptr		
			mov	a, _tmp1
			movx    @dptr, a	//	t->time = time
			sjmp	qt_99
		
//		if (pt->time <= time) {
//			time -= pt->time;
//			ptp = &pt->next;
//			pt = *ptp;
//			continue;
//		}
qt_98:
			dec	r0
			clr	a
			clr	c
			subb	a, _tmp0	//	time = -tmp //
			movx	@r0, a		// 
			inc	r0
			clr	a
			subb	a, _tmp1
			movx	@r0, a
			inc	dptr		
			mov	_DPL1, dpl	// prev = &pt->next
			mov	_DPH1, dph
			movx	a, @dptr	// pt = pt->next
			mov	r0, a
			inc	dptr		
			movx	a, @dptr
			mov	dpl, r0
			mov	dph, a
			mov	r2, #1
			ajmp	qt_11		//}


		
	__endasm;
}

void
cancel_task(task __xdata* t) __naked
{
	__asm;
	mov	r6, dpl		// 
	mov	r7, dph
	push	_IEN0		// s=EA;
	clr	EA		// EA = 0;
	mov	r2, #0
	mov	dpl, _waitq	// pt = waitq;
	mov	dph, _waitq+1
				// ptp = &waitq;
ct_1:				// for (;;) {
	//	
	mov	a, r6
	cjne	a, dpl, ct_2	//	if (pt == t) {
	mov	a, r7
	cjne	a, dph, ct_2
		inc	dptr
		inc	dptr
		movx	a, @dptr	// 	r1:r0 = t->time
		mov	r0, a
		inc	dptr
		movx	a, @dptr
		mov	r1, a
		inc	dptr
		movx	a, @dptr	// 	pt = t->next
		mov	r4, a
		inc	dptr
		movx	a, @dptr
		mov	r5, a
		inc	dptr
		mov	a, #TASK_IDLE	//	t->state = T_IDLE
		movx	@dptr, a

		cjne	r4, #0, ct_6	//      if (pt) {
		cjne	r5, #0, ct_6	
			sjmp	ct_3
ct_6:			mov	dpl, r4
			mov	dph, r5
			inc	dptr
			inc	dptr	//		pt->time += t->time;
			movx	a, @dptr
			add	a, r0
			movx	@dptr, a
			inc	dptr
			movx	a, @dptr
			addc	a, r1
			movx	@dptr, a
ct_3:					// 	}
		cjne	r2, #0, ct_8	//	*prev = pt
			mov	_waitq, r4	// 	waitq = dptr0
			mov	_waitq+1, r5
			pop	_IEN0	// EA = s;
			ret
ct_8:			mov 	_DPS, #1
			mov	a, r4 	//	prev->next = dptr0
			movx	@dptr, a
			inc	dptr
			mov	a, r5
			movx	@dptr, a
			mov 	_DPS, #0
			pop	_IEN0	// EA = s;
			ret
			
	//	}

ct_2:
	inc	dptr
	inc	dptr
	inc	dptr
	inc	dptr
	mov	_DPL1, dpl	// 	prev = &pt->next
	mov	_DPH1, dph
	movx	a, @dptr	// 	pt = pt->next
	inc	dptr
	mov	r2, a
	movx	a, @dptr
	mov	dph, a
	mov	dpl, r2
	mov	r2, #1
	sjmp	ct_1		// }
	__endasm;
}

#ifdef TASK_TEST
static unsigned char __xdata rgb[6]={0,0,0,0,0,0};
static void test(task __xdata*t);
static __xdata task test_task = {test,0,0,0};
static void test(task __xdata*t)
{
	rgb[0]+=16;
	rgb[1]+=16;
	send_RGB(&rgb[0]);
putstr("1");
	queue_task(&test_task, HZ);

}
static void test2(task __xdata*t);
static __xdata task test_task2 = {test2,0,0,0};
static void test2(task __xdata*t)
{
putstr("2");
	rgb[2]+=64;
	rgb[3]+=64;
	send_RGB(&rgb[0]);
	queue_task(&test_task2, 2*HZ);
}
static void test3(task __xdata*t);
static __xdata task test_task3 = {test3,0,0,0};
static void test3(task __xdata*t)
{
putstr("3");
	rgb[4]-=32;
	rgb[5]-=32;
	send_RGB(&rgb[0]);
	queue_task(&test_task3, 5*HZ/2);
}
#endif

#ifdef XMT
static void test4(task __xdata*t);
static __xdata task test_task4 = {test4,0,0,0};
static void test4(task __xdata*t)
{
	static __xdata struct {
		packet pkt;
		u8	data[5];
	} d;
	d.pkt.data[0] = 0x12;
	d.pkt.data[1] = 0x34;
	d.pkt.data[2] = 0x56;
	d.pkt.data[3] = 0x78;
	d.pkt.data[4] = 0x90;
	rf_send(&d.pkt, sizeof(d.pkt)-1+5, 0, 0);
	queue_task(&test_task4,  2*HZ);
}
#endif

void 
uart_init() __naked
{
	__asm;
        orl	_P0DIR, #0x08
        orl	_P0SEL, #0x08   // p0.3 out - TXD
        mov	_U0CSR, #0x80
        mov	_U0GCR, #0x08
        mov	_U0BAUD, #59
        mov	_U0CSR, #0x80
        //U0UCR = 0x82;
        mov	_U0UCR, #0x82
        mov	_UTX0IF, #0
	ret
	__endasm;
}

void
main()
{
	EA = 0;
	//
	CLKCONCMD=0x80;	// 32MHz ext, int 32khz
	while (CLKCONSTA != 0x80)
		;
	P1 = 0;
	P1DIR = 0xc1;
	P0DIR = 0x00;
        P0SEL = 0;
        P1SEL = 0;
        P2SEL = 0;
        P2DIR = 0;
        PERCFG = 0;     
	//
	// sleep timer
	//
	ST2 = 0;

#ifdef APP
	{
		extern code_hdr __code CODE_HEADER;
		x_app = (unsigned int (* __data ) (u8))(&CODE_HEADER.data[0]);
	}
#endif
	suota_setup();
	rf_init();
	(*x_app)(APP_INIT);

#ifdef XMT
	queue_task(&test_task4,  1*HZ);
#endif
#ifdef TASK_TEST
	queue_task(&test_task, 1*HZ);
	queue_task(&test_task2, 2*HZ);
	queue_task(&test_task3, 3*HZ);
#endif
	__asm;
	mov	r6, #0		//	delta = 0;
	mov	r7, #0	
m_1:				//  for (;;) {
distribute:
		mov	dpl, _waitq	//	for (xpt = waitq;;) {
		mov	dph, _waitq+1
m_2:			mov	a, dph
m_2a:			orl	a, dpl
			jz	m_4
			cjne	r6, #0, m_5//		if (!delta) break
			cjne	r7, #0, m_5
				sjmp	m_4
m_5:			inc	dptr	
			inc	dptr
			clr	c
			movx	a, @dptr//		delta = xpt->time = xpt->time-delta
			subb	a, r6
			movx	@dptr, a
			mov	r6, a
			inc	dptr
			movx	a, @dptr
			subb	a, r7	
			jc	m_6	//	 	if (xpt->time >= delta) {
				movx	@dptr, a//		xpt->time -= delta;
				sjmp	m_4//			break;
					//		}
m_6:			mov	r7, a
			clr	a
			movx    @dptr, a//		xpt->time = 0;
			dec	dpl
			mov	r0, dpl
			cjne	r0, #0xff, m_6a
				dec	dph
m_6a:			movx	@dptr, a
			clr	c
			subb	a, r6	// 		delta = -delta
			mov	r6, a
			clr	a
			subb	a, r7
			mov	r7, a
			inc	dptr
			inc	dptr
			movx 	a, @dptr//		xpt = xpt->next
			mov	r0, a
			inc	dptr
			movx 	a, @dptr
			mov	dpl, r0
			mov	dph, a
			sjmp	m_2a	//	}
m_4:					// for (;;) {
		mov	dpl, _waitq	//	xpt = waitq;
		mov	dph, _waitq+1
		mov	a, dpl		//	if (!xpt) break
		orl	a, dph
		jz	m_8
		mov	r6, dpl
		mov	r7, dph
		inc	dptr		//	if (xpt->time != 0) break
		inc	dptr
		movx 	a, @dptr
		cjne	a, #0, m_8
		inc	dptr
		movx 	a, @dptr
		cjne	a, #0, m_8
		push 	dpl
		push 	dph
		mov	dpl, #0xff	//	start_timer(60000);
		mov	dph, #0xfe
		acall	_start_timer
		pop 	dph
		pop 	dpl
		inc	dptr		//	waitq = xpt->next;
		movx	a, @dptr
		mov	_waitq, a
		inc	dptr		
		movx	a, @dptr
		mov	_waitq+1, a
		inc	dptr		
		mov	a, #TASK_CALLOUT//	xpt->state = TASK_CALLOUT;
		movx	@dptr, a
		setb	EA		//	EA = 1;
		push	dpl
		push	dph
	
		mov	a, #m_ret
		push 	acc
		mov	a, #m_ret>>8
		push 	acc
		mov	dpl, r6		//	xpt->callout(xpt);
		mov	dph, r7
		movx	a, @dptr
		push	acc
		inc	dptr
		movx	a, @dptr
		push	acc
		mov	dpl, r6		
		mov	dph, r7
		ret
m_ret:
		pop	dph
		pop	dpl
		clr	EA		//	EA = 0;
		movx	a, @dptr
		cjne	a, #TASK_CALLOUT, m_9//	if (xpt->state == TASK_CALLOUT)
			mov	a, #TASK_IDLE//		xpt->state = TASK_IDLE;
			movx	@dptr, a
m_9:
		acall	_stop_timer//	delta = stop_timer();
		mov	r6, dpl	
		mov	r7, dph	
		cjne	r6, #0, m_9a//	if (delta)
		cjne	r7, #0, m_9a//		goto distribute;
		sjmp	m_4		// }
m_9a:
			ajmp	distribute
m_8:
		mov	a, _waitq	// if (!waitq) {
		orl	a, _waitq+1
		jnz	m_10
			mov	_enter_sleep_mod_flag, #1//enter_sleep_mod_flag = 1;
			mov	_SLEEPCMD, #4 //	  SLEEPCMD = 4;//(radio_busy?0:2);
			setb	EA	//		  EA = 1;
m_8b:			mov	a, _enter_sleep_mod_flag
			cjne	a, #1, m_8a	// 	  while(enter_sleep_mod_flag) 
				acall	_sleep	//		sleep();
				sjmp	m_8b
m_8a:
			clr	EA	//		EA = 0;
			mov	r6, #0	//		delta = 0;
			mov	r7, #0
			ajmp	m_1
m_10:					//	} else {
			mov	dpl, _waitq
			mov	dph, _waitq+1
			inc	dptr
			inc	dptr
			movx	a, @dptr	//	if (!xpt->time) {
			mov	r0, a
			inc	dptr
			movx	a, @dptr
			cjne	a, #0, m_11
			cjne	r0, #0, m_11
				mov	r6, #0	//		delta = 0;
				mov	r7, #0
				ajmp	m_1
m_11:						//	} else {
				mov	_enter_sleep_mod_flag, #1	// enter_sleep_mod_flag = 1;
				mov	dpl, r0
				mov	dph, a
				acall 	_start_timer	// 	start_timer(xpt->time);
				mov	_SLEEPCMD, #4 //	SLEEPCMD = 4;//(radio_busy?0:2);
				setb	EA		//      EA = 1;
m_11b:				mov 	a, _enter_sleep_mod_flag
				cjne	a, #1, m_11a    //      while(enter_sleep_mod_flag) 
					acall	_sleep	//		sleep();
					sjmp	m_11b
m_11a:				clr	EA	//		EA = 0;
				acall	_stop_timer	//	delta = stop_timer();
				mov	r6, dpl
				mov	r7, dph
				ajmp	m_1		//  }
							// }
							//}
	__endasm;
}
