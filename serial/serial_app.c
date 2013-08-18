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

#define RX_SIZE 250
#define TX_SIZE 250
unsigned char __xdata rx_buff[RX_SIZE];
unsigned char __xdata tx_buff[TX_SIZE];
__xdata unsigned char *__data  rx_in = &rx_buff[0];
__xdata unsigned char *__pdata  rx_out = &rx_buff[0];
unsigned char __data  rx_count=0;
__xdata unsigned char *__pdata  tx_in = &tx_buff[0];
__xdata unsigned char *__data  tx_out = &tx_buff[0];
unsigned char __data  tx_count=TX_SIZE;
__bit tx_busy, rcv_busy;
void send_printf(char  __code *cp);
void tx_p(u8 c);

static void uart_rcv_thread(task __xdata*t);
static __xdata task uart_rcv_task = {uart_rcv_thread,0,0,0};

void ser_puthex(u8 v);
void ser_putstr(char __code *cp);
void ser_putc(char c);
extern void ps(const char __code *);
extern void pf(const char __code *);
extern void pd(unsigned char __xdata *, u8);
extern void pdd(unsigned char __pdata *, u8);
extern void ph(u8);

static void reset() __naked
{
	__asm;
	mov	_IEN0, #0
	ljmp	0
	__endasm;
}

static void tx_intr() __naked 
{
	__asm;
	push	PSW
	push	ACC
	clr	_UTX0IF
	mov	a, _tx_count
	cjne	a, #TX_SIZE, $0901

		clr	_tx_busy
		anl	_IEN2, #~(1<<2)
		pop	ACC
		pop	PSW
		reti
$0901:
	push	_DPS
	push	dpl
	push	dph
	mov	_DPS, #0
	mov	dpl, _tx_out
	mov	dph, _tx_out+1
	inc	a
	mov	_tx_count, a
	movx	a, @dptr
	mov	_U0DBUF, a 
	inc	dptr
	mov	a, #_tx_buff+TX_SIZE		// if (rx_in == &rx_buff[RX_SIZE]) 
	cjne	a, dpl, $0303
	mov	a, #(_tx_buff+TX_SIZE)>>8
	cjne	a, dph, $0303
		mov	dptr, #_tx_buff			//	rx_in = &rx_buff[0];
$0303:
	mov	_tx_out, dpl
	mov	_tx_out+1, dph

	pop	dph
	pop	dpl
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

$1101:
	mov	a, _U0CSR
	jb	_ACC_2, $1102	// U0CSR.RX_BYTE
		pop	acc
		pop 	PSW
		reti
$1102:
	mov	a, _rx_count
	cjne	a, #RX_SIZE, $0001				// if _rx_count == RX_SIZE return
		mov	a, _U0DBUF
		sjmp	$1101
$0001:		
	mov	a, _U0DBUF				// *_rx_in++ = U0DBUF
	push	_DPS
	push	dpl
	push	dph
	mov	_DPS, #0
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
	inc	_rx_count
	jb	_rcv_busy, $0005
		setb	_rcv_busy
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
			
		mov	dptr, #_uart_rcv_task		// queue_task_0(&uart_rcv_task, 0);
		lcall	_queue_task_0
	
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
$0005:	pop	dph
	pop	dpl
	pop	_DPS
	sjmp	$1101
	__endasm;
}

unsigned char __pdata rstate=0;
unsigned char __xdata keys[8][16];
unsigned char __xdata r[256];
unsigned char __pdata roff;
unsigned int  __pdata rsum;
unsigned char  __pdata rcmd;
unsigned char  __pdata rlen;
unsigned char  __pdata cs0;

void
ser_putstr(char __code *cp)
{
	char c;
	for (;;) {
		c = *cp++;
		if (!c)
			break;
		ser_putc(c);
	}
}

void
ser_puthex(u8 v)
{
	u8 x = (v>>4)&0xf;
	if (x < 10) {
		ser_putc(x+'0');
	} else {
		ser_putc(x+'a'-10);
	}
	x = v&0xf;
	if (x < 10) {
		ser_putc(x+'0');
	} else {
		ser_putc(x+'a'-10);
	}
}

static void uart_rcv_thread(task __xdata*t)
{
	for (;;) {
		unsigned char c;
		EA = 0;
		if (rx_count == 0) {
			rcv_busy = 0;
			EA = 1;
			return;
		}
		rx_count--;
		c = *rx_out;
		EA = 1;
		rx_out++;
		if (rx_out == &rx_buff[RX_SIZE])
			rx_out = &rx_buff[0];
		switch (rstate) {
		case 0: if (c != PKT_MAGIC_0)
				break;
			rstate = 1;
			break;
		case 1: if (c != PKT_MAGIC_1)
				break;
			rstate = 2;
			break;
		case 2: rcmd = c;
			rsum = c;
			rstate = 3;
			break;
		case 3: rlen = c;
			rsum += c;
			if (rlen != 0) {
				roff = 0;
				rstate = 4;
			} else {
				rstate = 5;
			}
			break;
		case 4:	r[roff++] = c;
			rsum += c;
			if (roff >= rlen)
				rstate = 5;
			break;
		case 5:	cs0 = c;
			rstate = 6;
			break;
		case 6: rstate = 0;
			if ((rsum&0xff) != cs0)
				break;
			if (((rsum>>8)&0xff) != c)
				break;
			switch (rcmd) {
			case PKT_CMD_PING:
				send_printf("PING\n");
				break;
			case PKT_CMD_RCV_OFF:
				rf_receive_off();
				break;
			case PKT_CMD_RCV_ON:
				rf_receive_on();
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
				rf_send((packet __xdata*)&r[0], rlen, NO_CRYPTO, 0);
				break;
			case PKT_CMD_SEND_PACKET_MAC:
				rf_send((packet __xdata*)&r[8], rlen-8, NO_CRYPTO, &r[0]);
				break;
			case PKT_CMD_SET_PROMISCUOUS:
				rf_set_promiscuous(r[0]);
				break;
			case PKT_CMD_RESET:
				reset();
				break;
			case PKT_CMD_SET_RAW:
				rf_set_raw(r[0]);
				break;
			default:
				if (rcmd >= PKT_CMD_SEND_PACKET_CRYPT && rcmd < (PKT_CMD_SEND_PACKET_CRYPT+8)) {
					int t = rcmd-PKT_CMD_SEND_PACKET_CRYPT;
					rf_send((packet __xdata*)&r[0], rlen, t, 0);
				} else
				if (rcmd >= PKT_CMD_SEND_PACKET_CRYPT_MAC && rcmd < (PKT_CMD_SEND_PACKET_CRYPT_MAC+8)) {					   
					int t = rcmd-PKT_CMD_SEND_PACKET_CRYPT_MAC;
					rf_send((packet __xdata*)&r[8], rlen-8, t, &r[0]);
				}
				break;
			}
			break;
		
		}
	}
}

void
tx_p(u8 c)
{
	*tx_in++ = c;
	if (tx_in == &tx_buff[TX_SIZE])
		tx_in = &tx_buff[0];
}

void
send_rcv_packet()
{
	unsigned int sum = rx_len+8;
	unsigned char xl = sum+6;
	unsigned char c, l;
	unsigned char  __xdata *d = (u8 * __xdata)rx_packet;

	if (xl > tx_count)
		return;
	tx_p(PKT_MAGIC_0);
	tx_p(PKT_MAGIC_1);
	{
		unsigned char cmd =
			rx_broadcast? (rx_crypto?PKT_CMD_RCV_PACKET_CRYPT_BROADCAST:PKT_CMD_RCV_PACKET_BROADCAST):
				      (rx_crypto?PKT_CMD_RCV_PACKET_CRYPT:PKT_CMD_RCV_PACKET);
		tx_p(cmd);
		sum += cmd;
	}
	tx_p(rx_len+8);
	l = 8;
	{
		unsigned char __xdata *mac = rx_mac;
		while (l--) {
			c = *mac++;
			sum += c;
			tx_p(c);
		}
	} 
	{
		unsigned char len = rx_len;
		while (len--) {
			c = *d++;
			sum += c;
			tx_p(c);
		}
	}
	tx_p(sum);
	tx_p(sum>>8);
	EA = 0;
	tx_count -= xl;
	if (!tx_busy) {
		tx_busy = 1;
		tx_count++;
		U0DBUF = *tx_out++;
		if (tx_out == &tx_buff[TX_SIZE])
			tx_out = &tx_buff[0];
		IEN2 |= 1<<2;
	}
	EA = 1;
}

void
ser_putc(char c)
{
	if (!tx_count)
		return;
	tx_p(c);
        EA = 0;
        tx_count--;
        if (!tx_busy) {
                tx_busy = 1;
                tx_count++;
                U0DBUF = *tx_out++;
		if (tx_out == &tx_buff[TX_SIZE])
			tx_out = &tx_buff[0];
                IEN2 |= 1<<2;
        }
        EA = 1;
}

void
send_printf(char  __code *cp)
{
	unsigned char len = strlen(cp)+1;
	unsigned char xl = len+6;
	unsigned int sum = PKT_CMD_PRINTF+len;
	unsigned char c;

	if (xl > tx_count)
		return;
	tx_p(PKT_MAGIC_0);
	tx_p(PKT_MAGIC_1);
	tx_p(PKT_CMD_PRINTF);
	tx_p(len);
	while (len--) {
		c = *cp++;
		sum += c;
		tx_p(c);
		if (!c)
			break;
	}
	tx_p(sum);
	tx_p(sum>>8);
	EA = 0;
	tx_count -= xl;
	if (!tx_busy) {
		tx_busy = 1;
		tx_count++;
		U0DBUF = *tx_out++;
		if (tx_out == &tx_buff[TX_SIZE])
			tx_out = &tx_buff[0];
		IEN2 |= 1<<2;
	}
	EA = 1;
}

void
send_printf_xdata(char  __xdata *cp)
{
	unsigned char len = strlen(cp)+1;
	unsigned char xl = len+6;
	unsigned int sum = PKT_CMD_PRINTF+len;
	unsigned char c;

	if (xl > tx_count)
		return;
	tx_p(PKT_MAGIC_0);
	tx_p(PKT_MAGIC_1);
	tx_p(PKT_CMD_PRINTF);
	tx_p(len);
	while (len--) {
		c = *cp++;
		sum += c;
		tx_p(c);
		if (!c)
			break;
	}
	tx_p(sum);
	tx_p(sum>>8);
	EA = 0;
	tx_count -= xl;
	if (!tx_busy) {
		tx_busy = 1;
		tx_count++;
		U0DBUF = *tx_out++;
		if (tx_out == &tx_buff[TX_SIZE])
			tx_out = &tx_buff[0];
		IEN2 |= 1<<2;
	}
	EA = 1;
}

char __xdata hex_tmp[3];

char
htoc(u8 v)
{
	v &=0xf;
	if (v < 10)
		return '0'+v;
	return ('a'-10)+v;
}
void
send_hex(u8 v)
{
	hex_tmp[0] = htoc(v>>4);
	hex_tmp[1] = htoc(v);
	hex_tmp[2] = 0;
	send_printf_xdata(&hex_tmp[0]);
}


static void
uart_setup()
{
	EA = 0;
	uart_rx_0_vect = rx_intr;
	uart_tx_0_vect = tx_intr;

	//IEN2.UTX0IE = 1;
        PERCFG  &= ~1;     // alt1 - USART0 on P0.3
        P0SEL |= 0x0c;   // p0.3 out - TXD
        P2DIR &= 0x3f;
        U0CSR = 0x80;
        U0GCR = 0x08;
        U0BAUD = 59;
        //U0UCR = 0x82;
        U0UCR = 0x82; // flush
        U0CSR = 0xdc;
        UTX0IF = 0;
	IEN0 |= 1<<2;
	EA = 1;
}


unsigned int my_app(unsigned char op) 
{
	switch (op) {
	case APP_INIT:
		// something to initialise variables - needs compiler hack
		uart_setup();
		send_printf("Startup\n");
		P2DIR |= 1;
		P2 |= 1;
		break;
	case APP_GET_MAC:
		return 0;
	case APP_GET_KEY:
		rf_set_key(&keys[rtx_key][0]);
		break;
	case APP_RCV_PACKET:
		send_rcv_packet();
		break;
	}
	return 0;
}

static char __xdata t[256];
__xdata static unsigned char *tc=&t[0];
void pc(char c)
{
	*tc++ = c;
}
void ps(const char __code *cp)
{
	char c;
	for (;;) {
		c = *cp++;
		if (!c)
			return;
		*tc++ = c;
	}
}
void pf(const char __code *cp)
{
	ps(cp);
	*tc = 0;
	send_printf_xdata(&t[0]);
	while (tx_busy);
	tc = &t[0];
}
void ph(u8 v)
{
	u8 x;
	char c;
	x =(v>>4)&0xf;
	if (x < 10) {
		c = '0'+x;
	} else {
		c = 'A'+x-10;
	}
	*tc++ = c;
	x =(v)&0xf;
	if (x < 10) {
		c = '0'+x;
	} else {
		c = 'A'+x-10;
	}
	*tc++ = c;
}
void pdd(unsigned char __pdata *s, u8 len)
{
	ph(len);
	pc(' ');
	ph(((unsigned int)s)>>8);
	ph((u8)s);
	pc(' ');
	while (len--)
		ph(*s++);

}
void pd(unsigned char __xdata *s, u8 len)
{
	ph(len);
	pc(' ');
	ph(((unsigned int)s)>>8);
	ph((u8)s);
	pc(' ');
	while (len--)
		ph(*s++);

}
