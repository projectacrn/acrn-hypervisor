/*-
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _VUART_H_
#define	_VUART_H_

struct fifo {
	char *buf;
	int rindex;	/* index to read from */
	int windex;	/* index to write to */
	int num;	/* number of characters in the fifo */
	int size;	/* size of the fifo */
};

struct vuart {
	char data;		/* Data register (R/W) */
	char ier;		/* Interrupt enable register (R/W) */
	char lcr;		/* Line control register (R/W) */
	char mcr;		/* Modem control register (R/W) */
	char lsr;		/* Line status register (R/W) */
	char msr;		/* Modem status register (R/W) */
	char fcr;		/* FIFO control register (W) */
	char scr;		/* Scratch register (R/W) */
	char dll;		/* Baudrate divisor latch LSB */
	char dlh;		/* Baudrate divisor latch MSB */

	struct fifo rxfifo;
	struct fifo txfifo;
	int base;

	bool thre_int_pending;	/* THRE interrupt pending */
	bool active;
	struct vm *vm;
	spinlock_t lock;	/* protects all softc elements */
};

#ifdef HV_DEBUG
void *vuart_init(struct vm *vm);
struct vuart *vuart_console_active(void);
void vuart_console_tx_chars(struct vuart *vu);
void vuart_console_rx_chars(struct vuart *vu);
#else
static inline void *vuart_init(__unused struct vm *vm)
{
	return NULL;
}
static inline struct vuart *vuart_console_active(void)
{
	return NULL;
}
static inline void vuart_console_tx_chars(__unused struct vuart *vu) {}
static inline void vuart_console_rx_chars(__unused struct vuart *vu) {}
#endif /*HV_DEBUG*/

#endif
