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
#include "interface.h"
#include "string.h"
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
unsigned char __xdata tx_buff[TX_SIZE];
__xdata unsigned char *__data  rx_in = &rx_buff[0];
__xdata unsigned char *__data  rx_out = &rx_buff[0];
unsigned char __data  rx_count=0;
__xdata unsigned char *__data  tx_in = &tx_buff[0];
__xdata unsigned char *__data  tx_out = &tx_buff[0];
unsigned char __data  tx_count=TX_SIZE;
__bit tx_busy;

static void uart_rcv_thread(task __xdata*t);
static __xdata task uart_rcv_task = {uart_rcv_thread,0,0,0};

static void tx_intr() __naked 
{
	__asm;
	push	PSW
	push	ACC
	mov	a, _tx_count
	cjne	a, #TX_SIZE, $0901

		clr	_tx_busy
		anl	_IEN2, #~(1<<2)
		pop	ACC
		pop	PSW
		reti
$0901:
	push	_DPS
	push	_DPH0
	push	_DPL0
	mov	_DPS, #0
	mov	dpl, _tx_out
	mov	dph, _tx_out+1
	inc	a
	mov	_tx_count, a
	movx	a, @dptr
	mov	_U0DBUF, a 
	inc	dptr
	mov	_tx_out, dpl
	mov	_tx_out+1, dph

	pop	_DPL0
	pop	_DPH0
	pop	_DPS
	
	pop	ACC
	pop	PSW
	reti
	__endasm;
}

static void rx_intr() __naked 
{
	__asm;
	push	PSW
	push	ACC
	mov	a, _rx_count
	cjne	a, #RX_SIZE, $0001				// if _rx_count == RX_SIZE return
		mov	a, _U0DBUF
		sjmp	$0002
$0001:		
		push	_DPS
		push	dpl
		push	dph
		mov	_DPS, #0
		mov	a, _U0DBUF				// *_rx_in++ = U0DBUF
		mov	dpl, _rx_in
		mov	dph, _rx_in+1
		movx	@dptr, a
		inc	dptr
		mov	a, #_rx_buff+RX_SIZE		// if (rx_in == &rx_buff[RX_SIZE]) 
		cjne	a, dpl, $0003
		mov	a, #(_rx_buff+RX_SIZE)>>8
		cjne	a, dph, $0003
			mov	dptr, #_rx_buff			//	rx_in = &rx_buff[0];
$0003:	
		mov	_rx_in, dpl
		mov	_rx_in+1, dph
		mov 	a, _rx_count				// if (rx_count++ == 0)
		jz	$0004
			inc	a
			mov 	_rx_count, a
			push	_DPL1
			push	_DPH1
	
			setb	_RS0	// save regs
			
			mov	dptr, #_uart_rcv_task		// queue_task_0(&uart_rcv_task, 0);
			lcall	_queue_task_0
	
			pop	_DPH1
			pop	_DPL1
			sjmp 	$0005
$0004:
			inc	a
			mov 	_rx_count, a

$0005:		pop	dph
		pop	dpl
		pop	_DPS
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
		IEN0 = s;
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
			if (off >= rlen)
				rstate = 5;
			break;
		case 5:	cs0 = c;
			rstate = 6;
			break;
		case 6: rstate = 0;
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
				rf_set_key(&keys[cur_key][0]);
				break;
			case PKT_CMD_SET_CHANNEL:
				rf_set_channel(r[0]);
				break;
			case PKT_CMD_SET_KEY:
				memcpy(&keys[r[0]&7][0], &r[1], 16);
				break;
			case PKT_CMD_SET_MAC:
				rf_set_mac(&r[0]);
				break;
			case PKT_CMD_SEND_PACKET:
				rf_send((packet __xdata*)&r[0], rlen, 0, 0);
				break;
			case PKT_CMD_SEND_PACKET_MAC:
				rf_send((packet __xdata*)&r[8], rlen-8, 0, &r[0]);
				break;
			default:
				if (cmd >= PKT_CMD_SEND_PACKET_CRYPT && cmd < (PKT_CMD_SEND_PACKET_CRYPT+8)) {					     int t = cmd-PKT_CMD_SEND_PACKET_CRYPT;
					if (cur_key != t) {
						cur_key = t;
						rf_set_key(&keys[t][0]);
					}
					rf_send((packet __xdata*)&r[0], rlen, 1, 0);
				} else
				if (cmd >= PKT_CMD_SEND_PACKET_CRYPT_MAC && cmd < (PKT_CMD_SEND_PACKET_CRYPT_MAC+8)) {					     int t = cmd-PKT_CMD_SEND_PACKET_CRYPT;
					if (cur_key != t) {
						cur_key = t;
						rf_set_key(&keys[t][0]);
					}
					rf_send((packet __xdata*)&r[8], rlen-8, 1, &r[0]);
				}
				break;
			}
			break;
		
		}
	}
}

void
send_rcv_packet(unsigned char  __xdata *mac, unsigned char __xdata *d, u8 len, u8 cmd)
{
	unsigned char xl = len+6+8;
	unsigned int sum = cmd+len+8;
	unsigned char c, l;

	if (xl > tx_count)
		return;
	*tx_in++ = PKT_MAGIC_0;
	*tx_in++ = PKT_MAGIC_1;
	*tx_in++ = cmd;
	*tx_in++ = len+8;
	l = 8;
	while (l--) {
		*tx_in++ = c = *mac++;
		sum += c;
	}
	while (len--) {
		*tx_in++ = c = *d++;
		sum += c;
	}
	*tx_in++ = sum;
	*tx_in++ = sum>>8;
	c = IEN0;
	IEN0 = 0;
	tx_count -= xl;
	if (!tx_busy) {
		tx_busy = 1;
		IEN2 |= 1<<2;
		tx_count++;
		U0DBUF = *tx_out++;
	}
	IEN0 = c;
}

static void
uart_setup()
{
	uart_rx_0_vect = rx_intr;
	uart_tx_0_vect = tx_intr;

	//IEN2.UTX0IE = 1;
        PERCFG  &= ~1;     // alt1 - USART0 on P0.3
        P0SEL |= 0x0c;   // p0.3 out - TXD
        P2DIR &= 0x3f;
        U0CSR = 0x80;
        U0GCR = 0x08;
        U0BAUD = 59;
        U0CSR = 0x80;
        //U0UCR = 0x82;
        U0UCR = 0x82; // flush
        UTX0IF = 0;

	U0CSR |= 0x40;	// enable receiver
	IEN0 |= 1<<2;
}


static unsigned int my_app(unsigned char op) 
{
	switch (op) {
	case APP_INIT:
		// something to initialise variables - needs compiler hack
		uart_setup();
		break;
	case APP_GET_MAC:
		return 0;
	case APP_GET_KEY:
		return (unsigned int)&keys[cur_key][0];
	case APP_RCV_PACKET:
		break;
	}
	return 0;
}

