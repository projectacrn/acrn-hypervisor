/*-
 * Copyright (c) 2012 NetApp, Inc.
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

#include <hypervisor.h>

#include "uart16550.h"

#define COM1_BASE	0x3F8
#define COM1_IRQ	4U
#define RX_FIFO_SIZE		256
#define TX_FIFO_SIZE		65536

#define vuart_lock_init(vu)	spinlock_init(&((vu)->lock))
#define vuart_lock(vu)		spinlock_obtain(&((vu)->lock))
#define vuart_unlock(vu)	spinlock_release(&((vu)->lock))

#define vm_vuart(vm)		(vm->vuart)

static void fifo_reset(struct fifo *fifo)
{
	fifo->rindex = 0;
	fifo->windex = 0;
	fifo->num = 0;
}

static void fifo_init(struct fifo *fifo, int sz)
{
	fifo->buf = calloc(1U, sz);
	ASSERT(fifo->buf != NULL, "");
	fifo->size = sz;
	fifo_reset(fifo);
}

static void fifo_putchar(struct fifo *fifo, char ch)
{
	fifo->buf[fifo->windex] = ch;
	if (fifo->num < fifo->size) {
		fifo->windex = (fifo->windex + 1) % fifo->size;
		fifo->num++;
	} else {
		fifo->rindex = (fifo->rindex + 1) % fifo->size;
		fifo->windex = (fifo->windex + 1) % fifo->size;
	}
}

static char fifo_getchar(struct fifo *fifo)
{
	char c;

	if (fifo->num > 0) {
		c = fifo->buf[fifo->rindex];
		fifo->rindex = (fifo->rindex + 1) % fifo->size;
		fifo->num--;
		return c;
	} else {
		return -1;
	}
}

static int fifo_numchars(struct fifo *fifo)
{
	return fifo->num;
}

/*
 * The IIR returns a prioritized interrupt reason:
 * - receive data available
 * - transmit holding register empty
 *
 * Return an interrupt reason if one is available.
 */
static uint8_t vuart_intr_reason(struct vuart *vu)
{
	if (((vu->lsr & LSR_OE) != 0) && ((vu->ier & IER_ELSI) != 0)) {
		return IIR_RLS;
	} else if ((fifo_numchars(&vu->rxfifo) > 0) && ((vu->ier & IER_ERBFI) != 0)) {
		return IIR_RXTOUT;
	} else if (vu->thre_int_pending && ((vu->ier & IER_ETBEI) != 0)) {
		return IIR_TXRDY;
	} else {
		return IIR_NOPEND;
	}
}

/*
 * Toggle the COM port's intr pin depending on whether or not we have an
 * interrupt condition to report to the processor.
 */
static void vuart_toggle_intr(struct vuart *vu)
{
	uint8_t intr_reason;

	intr_reason = vuart_intr_reason(vu);

	if (intr_reason != IIR_NOPEND) {
		if (vu->vm->vpic != NULL) {
			vpic_assert_irq(vu->vm, COM1_IRQ);
		}

		vioapic_assert_irq(vu->vm, COM1_IRQ);
		if (vu->vm->vpic != NULL) {
			vpic_deassert_irq(vu->vm, COM1_IRQ);
		}

		vioapic_deassert_irq(vu->vm, COM1_IRQ);
	}
}

static void vuart_write(__unused struct vm_io_handler *hdlr,
		struct vm *vm, uint16_t offset_arg,
		__unused size_t width, uint32_t value)
{
	uint16_t offset = offset_arg;
	struct vuart *vu = vm_vuart(vm);
	offset -= vu->base;
	vuart_lock(vu);
	/*
	 * Take care of the special case DLAB accesses first
	 */
	if ((vu->lcr & LCR_DLAB) != 0) {
		if (offset == UART16550_DLL) {
			vu->dll = value;
			goto done;
		}

		if (offset == UART16550_DLM) {
			vu->dlh = value;
			goto done;
		}
	}

	switch (offset) {
	case UART16550_THR:
		fifo_putchar(&vu->txfifo, value);
		vu->thre_int_pending = true;
		break;
	case UART16550_IER:
		/*
		 * Apply mask so that bits 4-7 are 0
		 * Also enables bits 0-3 only if they're 1
		 */
		vu->ier = value & 0x0FU;
		break;
	case UART16550_FCR:
		/*
		 * The FCR_ENABLE bit must be '1' for the programming
		 * of other FCR bits to be effective.
		 */
		if ((value & FCR_FIFOE) == 0U) {
			vu->fcr = 0;
		} else {
			if ((value & FCR_RFR) != 0U) {
				fifo_reset(&vu->rxfifo);
			}

			vu->fcr = value &
				(FCR_FIFOE | FCR_DMA | FCR_RX_MASK);
		}
		break;
	case UART16550_LCR:
		vu->lcr = value;
		break;
	case UART16550_MCR:
		/* ignore modem */
		break;
	case UART16550_LSR:
		/*
		 * Line status register is not meant to be written to
		 * during normal operation.
		 */
		break;
	case UART16550_MSR:
		/*
		 * As far as I can tell MSR is a read-only register.
		 */
		break;
	case UART16550_SCR:
		vu->scr = value;
		break;
	default:
		/*
		 * For the offset that is not handled (either a read-only
		 * register or an invalid register), ignore the write to it.
		 * Gracefully return if prior case clauses have not been met.
		 */
		break;
	}

done:
	vuart_toggle_intr(vu);
	vuart_unlock(vu);
}

static uint32_t vuart_read(__unused struct vm_io_handler *hdlr,
		struct vm *vm, uint16_t offset_arg,
		__unused size_t width)
{
	uint16_t offset = offset_arg;
	char iir, reg;
	uint8_t intr_reason;
	struct vuart *vu = vm_vuart(vm);
	offset -= vu->base;
	vuart_lock(vu);
	/*
	 * Take care of the special case DLAB accesses first
	 */
	if ((vu->lcr & LCR_DLAB) != 0) {
		if (offset == UART16550_DLL) {
			reg = vu->dll;
			goto done;
		}

		if (offset == UART16550_DLM) {
			reg = vu->dlh;
			goto done;
		}
	}
	switch (offset) {
	case UART16550_RBR:
		vu->lsr &= ~LSR_OE;
		reg = fifo_getchar(&vu->rxfifo);
		break;
	case UART16550_IER:
		reg = vu->ier;
		break;
	case UART16550_IIR:
		iir = ((vu->fcr & FCR_FIFOE) != 0) ? IIR_FIFO_MASK : 0;
		intr_reason = vuart_intr_reason(vu);
		/*
		 * Deal with side effects of reading the IIR register
		 */
		if (intr_reason == IIR_TXRDY) {
			vu->thre_int_pending = false;
		}
		iir |= intr_reason;
		reg = iir;
		break;
	case UART16550_LCR:
		reg = vu->lcr;
		break;
	case UART16550_MCR:
		reg = vu->mcr;
		break;
	case UART16550_LSR:
		/* Transmitter is always ready for more data */
		vu->lsr |= LSR_TEMT | LSR_THRE;
		/* Check for new receive data */
		if (fifo_numchars(&vu->rxfifo) > 0) {
			vu->lsr |= LSR_DR;
		} else {
			vu->lsr &= ~LSR_DR;
		}
		reg = vu->lsr;
		/* The LSR_OE bit is cleared on LSR read */
		vu->lsr &= ~LSR_OE;
		break;
	case UART16550_MSR:
		/* ignore modem I*/
		reg = 0;
		break;
	case UART16550_SCR:
		reg = vu->scr;
		break;
	default:
		reg = 0xFF;
		break;
	}
done:
	vuart_toggle_intr(vu);
	vuart_unlock(vu);
	return reg;
}

static void vuart_register_io_handler(struct vm *vm)
{
	struct vm_io_range range = {
		.flags = IO_ATTR_RW,
		.base = COM1_BASE,
		.len = 8U
	};

	register_io_emulation_handler(vm, &range, vuart_read, vuart_write);
}

/**
 * @pre vu != NULL
 */
void vuart_console_tx_chars(struct vuart *vu)
{
	vuart_lock(vu);
	while (fifo_numchars(&vu->txfifo) > 0) {
		printf("%c", fifo_getchar(&vu->txfifo));
	}
	vuart_unlock(vu);
}

/**
 * @pre vu != NULL
 * @pre vu->active == true
 */
void vuart_console_rx_chars(struct vuart *vu)
{
	char ch = -1;

	vuart_lock(vu);
	/* Get data from physical uart */
	ch = uart16550_getc();

	if (ch == GUEST_CONSOLE_TO_HV_SWITCH_KEY) {
		/* Switch the console */
		vu->active = false;
		printf("\r\n\r\n ---Entering ACRN SHELL---\r\n");
	}
	if (ch != -1) {
		fifo_putchar(&vu->rxfifo, ch);
		vuart_toggle_intr(vu);
	}

	vuart_unlock(vu);
}

struct vuart *vuart_console_active(void)
{
	struct vm *vm = get_vm_from_vmid(0U);

	if ((vm != NULL) && (vm->vuart != NULL)) {
		struct vuart *vu = vm->vuart;

		if (vu->active) {
			return vm->vuart;
		}
	}
	return NULL;
}

void *vuart_init(struct vm *vm)
{
	struct vuart *vu;
	uint16_t divisor;

	vu = calloc(1U, sizeof(struct vuart));
	ASSERT(vu != NULL, "");

	/* Set baud rate*/
	divisor = UART_CLOCK_RATE / BAUD_9600 / 16U;
	vu->dll = divisor;
	vu->dlh = divisor >> 16U;

	vu->active = false;
	vu->base = COM1_BASE;
	vu->vm = vm;
	fifo_init(&vu->rxfifo, RX_FIFO_SIZE);
	fifo_init(&vu->txfifo, TX_FIFO_SIZE);
	vuart_lock_init(vu);
	vuart_register_io_handler(vm);

	return vu;
}
