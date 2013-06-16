#include "interface.h"

#define SCAN_RATE HZ/4
#define THRES_O		4
#define THRES_X		4
#define THRES_LT	4
#define THRES_GT	4

static void keys_thread(task __xdata*t);
static __xdata task key_task = {keys_thread,0,0,0};
static __data u8 key_state;
u8 __pdata key;  
__bit key_down;
static __bit key0_pressed, key1_pressed, key2_pressed, key3_pressed;

void
keys_off() __naked
{
	__asm;
	mov	_T1CTL, #0
	mov	dptr, #_key_task	// cancel_task(&key_task);
	ljmp	_cancel_task
	__endasm;
}

void
keys_on() __naked
{
	__asm;
	clr	_key0_pressed
	clr	_key1_pressed
	clr	_key2_pressed
	clr	_key3_pressed
	clr	a
	mov	_T1CTL, a
	mov	_T1CCTL0, a
	mov	_T1CCTL1, a
	mov	_T1CCTL2, a
	mov	_key_state, a
	mov	dptr, #_T1CCTL3
	movx	@dptr, a
	inc	dptr	// _T1CCTL4
	movx	@dptr, a
	mov	a, #0x74
	orl	_P0DIR, a	// these are outputs
	orl	_P0INP, a	// tristate
	anl	_P0SEL, #~0x74
	orl	_PERCFG, #2
	setb	_P0_2
	setb	_P0_3
	setb	_P0_4
	setb	_P0_5
	setb	_P0_6
	sjmp	_keys_thread		//	keys(&key_task);
	__endasm;
}

//
//	wiring:
//		[O] - P0.2	timer1 input 0
//		[X] - P0.4	timer1 input 2
//		[<] - P0.5	timer1 input 3
//		[>] - P0.6	timer1 input 4
//
//	To sample an input:
//	- drive it and its pair high
//	- tristate its input
//	- set timer input to it's matching input
//	- pull its pair low
//	- time time until timer is triggered
//

static void call_key_app() __naked
{
	__asm;
	mov	r0, #_key
	movx	@r0, a
	mov	dpl, #APP_KEY
	push	_x_app		//jump to app
	push	_x_app+1
	ret
	__endasm;
}

static void
keys_thread(task __xdata*ts) __naked
{
	__asm;
//putstr("@");puthex(key_state);putstr("\n\n");
//	switch (key_state) {
//	case 0x00:
	mov	a, _key_state
	jnz	$0001
//		key_state = 0x04;
//		T1CCTL0 = 0x42;
//		PERCFG |= 3;
//		P0DIR &= ~0x04;	// trisate sample pin
//		P0_4 =  0;
//		break;
		mov	_key_state, #0x04
		mov	_T1CCTL0, #0x42
		orl	_PERCFG, #3
  		anl	_P0SEL, #~0x8
		anl	_P0DIR, #~0x04;	// trisate sample pin
		clr	_P0_4 
		ajmp	$0009
$0001:
//	case 0x04:
	jnb	_ACC_2, $0002
//		T1CCTL0 = 0x00;
//		if (T1CC0H > 5) {
//			if (!key0_pressed) {
//				key_down = 1;
//				key0_pressed = 1;
//				key = KEY_O;
//				call_key_app();
//			}
//		} else {
//			if (key0_pressed) {
//				key_down = 0;
//				key0_pressed = 0;
//				key = KEY_O;
//				call_key_app();
//			}
//		}
		mov	_T1CCTL0, #0
		mov	a, _T1CC0H
		add	a, #0xff-THRES_O
$0013:		
		jnc	$0011
			jb	_key0_pressed, $0012
				setb	_key_down
				setb	_key0_pressed
				sjmp	$0014
$0011:
			jnb	_key0_pressed, $0012
				clr	_key_down
				clr	_key0_pressed
$0014:
				mov	a, #KEY_O
				acall	_call_key_app
$0012:		
		//putstr("key ");puthex(key_state);putstr(" ");puthex(T1CC0H);puthex(T1CC0L);putstr("\n");
//		P0_4 =  1;
//		T1CCTL2 = 0x42;
//		key_state = 0x10;
//		PERCFG |= 3;
//		P0DIR &= ~0x10;	// trisate sample pin
//		P0_2 =  0;
//		break;
		setb	_P0_4
		mov	_T1CCTL2, #0x42
		mov	_key_state, #0x10
  		anl	_P0SEL, #~0x8
		orl	_PERCFG, #3
		anl	_P0DIR, #~0x10	// trisate sample pin
		clr	_P0_2
		ajmp	$0009

$0002:
//	case 0x10:
	jnb	_ACC_4, $0003
//		T1CCTL2 = 0x00;
//		if (T1CC2H > 5) {
//			if (!key1_pressed) {
//				key_down = 1;
//				key1_pressed = 1;
//				key = KEY_X;
//				call_key_app();
//			}
//		} else {
//			if (key1_pressed) {
//				key_down = 0;
//				key1_pressed = 0;
//				key = KEY_X;
//				call_key_app();
//			}
//		}
		mov	_T1CCTL2, #0
		mov	a, _T1CC2H
		add	a, #0xff-THRES_X
		jnc	$0021
			jb	_key1_pressed, $0022
				setb	_key_down
				setb	_key1_pressed
				sjmp	$0024
$0021:
			jnb	_key1_pressed, $0022
				clr	_key_down
				clr	_key1_pressed
$0024:
				mov	a, #KEY_X
				acall	_call_key_app
$0022:		
		//putstr("key ");puthex(key_state);putstr(" ");puthex(T1CC2H);puthex(T1CC2L);putstr("\n");
//		P0_2 =  1;
//		T1CCTL3 = 0x42;
//		key_state = 0x20;
//		PERCFG |= 3;
//		P0DIR &= ~0x20;	// trisate sample pin
//		P0_6 =  0;
//		break;
		setb	_P0_2
		mov	dptr, #_T1CCTL3
		mov	a, #0x42
		movx	@dptr, a
		mov	_key_state, #0x20
  		anl	_P0SEL, #~0x8
		orl	_PERCFG, #3
		anl	_P0DIR, #~0x20	// trisate sample pin
		clr	_P0_6
		sjmp	$0009
$0003:
//	case 0x20:
	jnb	_ACC_5, $0004
//		T1CCTL3 = 0x00;
//		if (T1CC3H > 5) {
//			if (!key2_pressed) {
//				key_down = 1;
//				key3_pressed = 1;
//				key = KEY_LEFT;
//				call_key_app();
//			}
//		} else {
//			if (key2_pressed) {
//				key_down = 0;
//				key3_pressed = 0;
//				key = KEY_LEFT;
//				call_key_app();
//			}
//		}
		mov	dptr, #_T1CCTL3
		clr	a
		movx	@dptr, a
		mov	dptr, #_T1CC3H
		movx	a, @dptr
		add	a, #0xff-THRES_LT
		jnc	$0031
			jb	_key2_pressed, $0032
				setb	_key_down
				setb	_key2_pressed
				sjmp	$0034
$0031:
			jnb	_key2_pressed, $0032
				clr	_key_down
				clr	_key2_pressed
$0034:
				mov	a, #KEY_LEFT
				acall	_call_key_app
$0032:		
//		//putstr("key ");puthex(key_state);putstr(" ");puthex(T1CC3H);puthex(T1CC3L);putstr("\n");
//		P0_6 =  1;
//		T1CCTL4 = 0x42;
//		key_state = 0x40;
//		P0DIR &= ~0x40;	// trisate sample pin
//		P0_5 =  0;
//		break;
		setb	_P0_6
		mov	dptr, #_T1CCTL4
		mov	a, #0x42
		movx	@dptr, a
		mov	_key_state, #0x40
  		anl	_P0SEL, #~0x8
		anl	_P0DIR, #~0x40	// trisate sample pin
		clr	_P0_5
		sjmp	$0009
$0004:
//	case 0x40:
	jnb	_ACC_6, $0008
//		if (T1CC4H > 5) {
//			if (!key3_pressed) {
//				key_down = 1;
//				key3_pressed = 1;
//				key = KEY_RIGHT;
//				call_key_app();
//			}
//		} else {
//			if (key3_pressed) {
//				key_down = 0;
//				key3_pressed = 0;
//				key = KEY_RIGHT;
//				call_key_app();
//			}
//		}
		mov	dptr, #_T1CCTL4
		clr	a
		movx	@dptr, a
		mov	dptr, #_T1CC4H
		movx	a, @dptr
		add	a, #0xff-THRES_GT
		jnc	$0041
			jb	_key3_pressed, $0042
				setb	_key_down
				setb	_key3_pressed
				sjmp	$0044
$0041:
			jnb	_key3_pressed, $0042
				clr	_key_down
				clr	_key3_pressed
$0044:
				mov	a, #KEY_RIGHT
				acall	_call_key_app
$0042:		
		//putstr("key ");puthex(key_state);putstr(" ");puthex(T1CC4H);puthex(T1CC4L);putstr("\n");
//		P0_5 =  1;
//		key_state = 0;
//		queue_task(&key_task, HZ);	// poll again in 1 seconds
//		return;
		setb	_P0_5
		mov	_key_state, #0
		mov	dptr, #_key_task
		mov	r0, #_queue_task_PARM_2	//	time = HZ;
		mov	a, #(SCAN_RATE)
		movx	@r0, a
		inc	r0
		mov	a, #(SCAN_RATE)>>8
		movx	@r0, a
		ljmp	_queue_task
//	}
$0009:
	clr	a
	mov	_T1CNTL, a
	mov	_T1STAT, a
	mov	_T1CTL, #0x01		// start counter
	orl	_IEN1, #0x02
$0008:	ret
	__endasm;
}

void
t1_isr()  __interrupt(9) __naked
{
	__asm;
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
	push	_PSW
	mov	_T1CTL, #0
	anl	_PERCFG, #~0x01
  	orl _P0SEL, #0x8
	anl	_IEN1, #~0x02
	orl	_P0DIR, #0x74;
	orl 	_P0INP, #0x74;	// tristate
	push	acc
	push	dpl
	push	dph
	push	_DPL1
	push	_DPH1
	push	_DPS

	setb	_RS0	// save regs
	mov	_DPS, #0
	mov	dptr, #_key_task
	lcall	_queue_task_0		//	queue_task(&key_task, 0);
	pop	_DPS
	pop	_DPH1
	pop	_DPL1
	pop	dph
	pop	dpl
	pop	acc
	pop	_PSW
	reti
	__endasm;
}

