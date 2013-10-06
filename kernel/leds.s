	; r1 has x1
	; r2 has x2
DELAY1 = 1
DELAY2 = 1
DELAY3 = 1
DPS    =       0x0092

	.area XSEG    (DATA)
	.globl	_led_tmp
_led_tmp::	.ds 24
	.area CSEG    (CODE)
	.globl	_leds_rgb
	.globl	_leds_off

_leds_off:
	mov	P1, #0
	ret
_leds_rgb:
	mov	r4, #3
	mov	DPS, #1
	mov	dptr, #_led_tmp
loop_rgb:
		mov	DPS, #0
		movx	a, @dptr
		mov	r6, a
		inc	dptr
		movx	a, @dptr
		mov	r7, a
		inc	dptr
		mov	DPS, #1
	
		mov	r3, #8
loop_bits:
		mov	a, r6	
		rlc	a
		mov	r6, a
		jnc	m2
			mov	a, r7	
			rlc	a
			mov	r7, a
			jnc	m3
				mov 	a, #0xc1
				sjmp	m5
m3:				mov 	a, #0x81
				sjmp	m5
m2:			mov	a, r7	
			rlc	a
			mov	r7, a
			jnc	m4
				mov 	a, #0xc0
				sjmp	m5
m4:				mov 	a, #0x80
m5:
			movx 	@dptr, a
			inc	dptr
			djnz	r3, loop_bits
		djnz	r4, loop_rgb

	mov	DPS, #0
	mov	dptr, #_led_tmp
	mov	r1, #24
	mov	IE, #0
loop:
		mov	P1, #0xc1	; high
		movx	a, @dptr
		nop
		nop
		nop
		nop
		mov	P1, a	
		inc	dptr
		dec	r2
		mov 	P1, #0x80
		mov	r0, #1
l1:			djnz    r0, l1
		dec	r2
		djnz    r1, loop			
	mov	IE, #1
	ret
