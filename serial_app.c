
#include <mcs51reg.h>
#include <cc2530.h>
#include "interface.h"
#include "packet_interface.h"

static xxxx() __naked {
	__asm;
	.area CSEG    (CODE)
	.globl	_CODE_HEADER
_CODE_HEADER:
	.db	0, 0, 0, 0	// CRC will go here
	.db	THIS_ARCH
	.db	0, 0, 0		// version (little endian)
	.db	0, 0		// len from arch to end of code 
	ljmp	_my_app
	__endasm;
}

#define RX_SIZE 250
#define TX_SIZE 250
unsigned char __xdata rx_buff[RX_SIZE];
unsigned char __xdata tx_buff[RX_SIZE];
__xdata unsigned char *__data  rx_in = &rx_buff[0];
__xdata unsigned char *__data  rx_out = &rx_buff[0];
unsigned char __data  rx_count=0;
__xdata unsigned char *__data  tx_in = &tx_buff[0];
__xdata unsigned char *__data  tx_out = &tx_buff[0];
unsigned char __data  tx_count=0;

static void uart_rcv_thread(task __xdata*t);
static __xdata task uart_rcv_task = {uart_rcv_thread,0,0,0};

static void rx_intr() __naked 
{
	__asm;
	push	PSW
	push	ACC
	mov	a, _rx_count
	cjne	a, #RX_SIZE, $0001				// if _rx_count == RX_SIZE return
		mov	a, U0DBUF
		sjmp	$0002
$0001:		
		push	DPS
		push	dpl
		push	dph
		mov	DPS, #0
		mov	a, U0DBUF				// *_rx_in++ = U0DBUF
		mov	dpl, _rx_in
		mov	dph, _rx_in+1
		movx	@dptr, a
		inc	dptr
		mov	a, #(_rx_buff+RX_SIZE)&0xff		// if (rx_in == &rx_buff[RX_SIZE]) 
		cjne	a, dpl, $0003
		mov	a, #((_rx_buff+RX_SIZE)>>8)&0xff
		cjne	a, dph, $0003
			mov	dptr, #_rx_buff			//	rx_in = &rx_buff[0];
$0003:	
		mov	_rx_in, dpl
		mov	_rx_in+1, dph
		mov 	a, _rx_count				// if (rx_count++ == 0)
		jz	$0004
			inc	a
			mov 	_rx_count, a
			push	DPL1
			push	DPH1
	
			setb	_RS0	// save regs
			
			mov	dptr, #_uart_rcv_task		// queue_task_0(&uart_rcv_task, 0);
			lcall	_queue_task_0
	
			pop	DPH1
			pop	DPL1
			sjmp 	$0005
$0004:
			inc	a
			mov 	_rx_count, a

$0005:		pop	dph
		pop	dpl
		pop	DPS
$0002:
	pop	acc
	pop 	PSW
	reti
	__endasm;
}

unsigned char __data rstate=0;
unsigned char __xdata keys[8][16];
unsigned char __xdata r[256];
unsigned char __data off;
unsigned int  __data sum;
unsigned char  __data cmd;
unsigned char  __data rlen;
unsigned char  __data cs0;
unsigned char __data cur_key=0xff;

static void uart_rcv_thread(task __xdata*t)
{
	for (;;) {
		unsigned char c, s;
		if (rx_count == 0)
			return;
		s = IEN0;
		IEN0 = 0;
		c = *rx_out;
		rx_out++;
		if (rx_out == &rx_buff[RX_SIZE])
			rx_out = &rx_buff[0];
		rx_count--;
		IEN0 = a;
		switch (rstate) {
		case 0: if (c != PKT_MAGIC_0)
				break;
			rstate = 1;
			break;
		case 1: if (c != PKT_MAGIC_1)
				break;
			rstate = 2;
			break;
		case 2: cmd = c;
			sum = c;
			rstate = 3;
			break;
		case 3: rlen = c;
			sum += c;
			if (rlen != 0) {
				off = 0;
				rstate = 4;
			} else {
				rstate = 5;
			}
			break;
		case 4:	r[off++] = c;
			sum += c;
			if (off >= len)
				rstate = 5;
			break;
		case 5:	cs0 = c;
			state = 6;
			break;
		case 6: state = 0;
			if ((sum&0xff) != c)
				break;
			if (((sum>>8)&0xff) != cs0)
				break;
			switch (cmd) {
			case PKT_CMD_RCV_OFF:
				rf_receive_off();
				break;
			case PKT_CMD_RCV_ON:
				rf_receive_on();
				cur_key = r[0];
				rf_set_key(&keys[cur_key]);
				break;
			case PKT_CMD_SET_CHANNEL:
				rf_set_channel(r[0]);
				break;
			case PKT_CMD_SET_KEY:
				memcpy(&keys[r[0]&7], &r[1], 16);
				break;
			case PKT_CMD_SET_MAC:
				rf_set_mac(&r[0]);
				break;
			case PKT_CMD_SEND_PACKET:
				rf_send(&r[0], rlen, 0);
				break;
			default:
				if (cmd >= PKT_CMD_SEND_PACKET_CRYPT && cmd < (PKT_CMD_SEND_PACKET_CRYPT+8)) {					     int t = cmd-PKT_CMD_SEND_PACKET_CRYPT;
					if (cur_key != t) {
						cur_key = t;
						rf_set_key(&keys[t][0]);
					}
					rf_send(&r[0], rlen, 1);
				}
				break;
			}
			break;
		
		}
	}
}

static void
uart_init()
{
	PERCFG &= ~1;
	P0SEL |= 0x0c;
	P2DIR &= 0x3f;
	uart_rx_0_vect = rx_intr;
	UxUCR0.FLUSH = 1
	UxCSR0.RE = 1;
	IEN0.URX0IE = 1;

	//IEN2.UTX0IE = 1;
}


static unsigned int my_app(unsigned char op) 
{
	switch (op) {
	case APP_INIT:
		// something to initialise variables - needs compiler hack
		uart_init();
		break;
	case APP_GET_MAC:
		return (unsigned int)&mac[0];
	case APP_GET_KEY:
		return (unsigned int)&key[cur_key][0]);
	case APP_RCV_PACKET:
		break;
	}
	return 0;
}

