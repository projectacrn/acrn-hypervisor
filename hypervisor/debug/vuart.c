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


#ifndef CONFIG_PARTITION_MODE
static char vuart_rx_buf[RX_BUF_SIZE];
static char vuart_tx_buf[TX_BUF_SIZE];
#endif

#define vuart_lock_init(vu)	spinlock_init(&((vu)->lock))
#define vuart_lock(vu)		spinlock_obtain(&((vu)->lock))
#define vuart_unlock(vu)	spinlock_release(&((vu)->lock))

#ifdef CONFIG_PARTITION_MODE
int8_t vuart_vmid = - 1;
#endif

static inline void fifo_reset(struct fifo *fifo)
{
	fifo->rindex = 0U;
	fifo->windex = 0U;
	fifo->num = 0U;
}

static inline void fifo_putchar(struct fifo *fifo, char ch)
{
	fifo->buf[fifo->windex] = ch;
	if (fifo->num < fifo->size) {
		fifo->windex = (fifo->windex + 1U) % fifo->size;
		fifo->num++;
	} else {
		fifo->rindex = (fifo->rindex + 1U) % fifo->size;
		fifo->windex = (fifo->windex + 1U) % fifo->size;
	}
}

static inline char fifo_getchar(struct fifo *fifo)
{
	char c;

	if (fifo->num > 0U) {
		c = fifo->buf[fifo->rindex];
		fifo->rindex = (fifo->rindex + 1U) % fifo->size;
		fifo->num--;
		return c;
	} else {
		return -1;
	}
}

static inline uint32_t fifo_numchars(const struct fifo *fifo)
{
	return fifo->num;
}

static inline void vuart_fifo_init(struct acrn_vuart *vu)
{
#ifdef CONFIG_PARTITION_MODE
	vu->txfifo.buf = vu->vuart_tx_buf;
	vu->rxfifo.buf = vu->vuart_rx_buf;
#else
	vu->txfifo.buf = vuart_tx_buf;
	vu->rxfifo.buf = vuart_rx_buf;
#endif
	vu->txfifo.size = TX_BUF_SIZE;
	vu->rxfifo.size = RX_BUF_SIZE;
	fifo_reset(&(vu->txfifo));
	fifo_reset(&(vu->rxfifo));
}

/*
 * The IIR returns a prioritized interrupt reason:
 * - receive data available
 * - transmit holding register empty
 *
 * Return an interrupt reason if one is available.
 */
static uint8_t vuart_intr_reason(const struct acrn_vuart *vu)
{
	if (((vu->lsr & LSR_OE) != 0U) && ((vu->ier & IER_ELSI) != 0U)) {
		return IIR_RLS;
	} else if ((fifo_numchars(&vu->rxfifo) > 0U) &&
					((vu->ier & IER_ERBFI) != 0U)) {
		return IIR_RXTOUT;
	} else if (vu->thre_int_pending && ((vu->ier & IER_ETBEI) != 0U)) {
		return IIR_TXRDY;
	} else {
		return IIR_NOPEND;
	}
}

/*
 * Toggle the COM port's intr pin depending on whether or not we have an
 * interrupt condition to report to the processor.
 */
static void vuart_toggle_intr(const struct acrn_vuart *vu)
{
	uint8_t intr_reason;
	union ioapic_rte rte;
	uint32_t operation;

	intr_reason = vuart_intr_reason(vu);
	vioapic_get_rte(vu->vm, CONFIG_COM_IRQ, &rte);

	/* TODO:
	 * Here should assert vuart irq according to CONFIG_COM_IRQ polarity.
	 * The best way is to get the polarity info from ACIP table.
	 * Here we just get the info from vioapic configuration.
	 * based on this, we can still have irq storm during guest
	 * modify the vioapic setting, as it's only for debug uart,
	 * we want to make it as an known issue.
	 */
	if ((rte.full & IOAPIC_RTE_INTPOL) != 0UL) {
		operation = (intr_reason != IIR_NOPEND) ?
				GSI_SET_LOW : GSI_SET_HIGH;
	} else {
		operation = (intr_reason != IIR_NOPEND) ?
				GSI_SET_HIGH : GSI_SET_LOW;
	}

	vpic_set_irq(vu->vm, CONFIG_COM_IRQ, operation);
	vioapic_set_irq(vu->vm, CONFIG_COM_IRQ, operation);
}

static void vuart_write(struct acrn_vm *vm, uint16_t offset_arg,
			__unused size_t width, uint32_t value)
{
	uint16_t offset = offset_arg;
	struct acrn_vuart *vu = vm_vuart(vm);
	uint8_t value_u8 = (uint8_t)value;

	offset -= vu->base;
	vuart_lock(vu);
	/*
	 * Take care of the special case DLAB accesses first
	 */
	if ((vu->lcr & LCR_DLAB) != 0U) {
		if (offset == UART16550_DLL) {
			vu->dll = value_u8;
			goto done;
		}

		if (offset == UART16550_DLM) {
			vu->dlh = value_u8;
			goto done;
		}
	}

	switch (offset) {
	case UART16550_THR:
		fifo_putchar(&vu->txfifo, (char)value_u8);
		vu->thre_int_pending = true;
		break;
	case UART16550_IER:
		/*
		 * Apply mask so that bits 4-7 are 0
		 * Also enables bits 0-3 only if they're 1
		 */
		vu->ier = value_u8 & 0x0FU;
		break;
	case UART16550_FCR:
		/*
		 * The FCR_ENABLE bit must be '1' for the programming
		 * of other FCR bits to be effective.
		 */
		if ((value_u8 & FCR_FIFOE) == 0U) {
			vu->fcr = 0U;
		} else {
			if ((value_u8 & FCR_RFR) != 0U) {
				fifo_reset(&vu->rxfifo);
			}

			vu->fcr = value_u8 &
				(FCR_FIFOE | FCR_DMA | FCR_RX_MASK);
		}
		break;
	case UART16550_LCR:
		vu->lcr = value_u8;
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
		vu->scr = value_u8;
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

static uint32_t vuart_read(struct acrn_vm *vm, uint16_t offset_arg,
			__unused size_t width)
{
	uint16_t offset = offset_arg;
	uint8_t iir, reg, intr_reason;
	struct acrn_vuart *vu = vm_vuart(vm);

	offset -= vu->base;
	vuart_lock(vu);
	/*
	 * Take care of the special case DLAB accesses first
	 */
	if ((vu->lcr & LCR_DLAB) != 0U) {
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
		reg = (uint8_t)fifo_getchar(&vu->rxfifo);
		break;
	case UART16550_IER:
		reg = vu->ier;
		break;
	case UART16550_IIR:
		iir = ((vu->fcr & FCR_FIFOE) != 0U) ? IIR_FIFO_MASK : 0U;
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
		if (fifo_numchars(&vu->rxfifo) > 0U) {
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
		reg = 0U;
		break;
	case UART16550_SCR:
		reg = vu->scr;
		break;
	default:
		reg = 0xFFU;
		break;
	}
done:
	vuart_toggle_intr(vu);
	vuart_unlock(vu);
	return (uint32_t)reg;
}

static void vuart_register_io_handler(struct acrn_vm *vm)
{
	struct vm_io_range range = {
		.flags = IO_ATTR_RW,
		.base = CONFIG_COM_BASE,
		.len = 8U
	};

	register_io_emulation_handler(vm, UART_PIO_IDX, &range, vuart_read, vuart_write);
}

/**
 * @pre vu != NULL
 */
void vuart_console_tx_chars(struct acrn_vuart *vu)
{
	vuart_lock(vu);
	while (fifo_numchars(&vu->txfifo) > 0U) {
		printf("%c", fifo_getchar(&vu->txfifo));
	}
	vuart_unlock(vu);
}

/**
 * @pre vu != NULL
 * @pre vu->active == true
 */
void vuart_console_rx_chars(struct acrn_vuart *vu)
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

struct acrn_vuart *vuart_console_active(void)
{
#ifdef CONFIG_PARTITION_MODE
	struct acrn_vm *vm;

	if (vuart_vmid == -1) {
		return NULL;
	}

	vm = get_vm_from_vmid(vuart_vmid);
#else
	struct acrn_vm *vm = get_vm_from_vmid(0U);
#endif

	if (vm != NULL) {
		struct acrn_vuart *vu = vm_vuart(vm);

		if (vu->active) {
			return vu;
		}
	}
	return NULL;
}

void vuart_init(struct acrn_vm *vm)
{
	uint32_t divisor;
	struct acrn_vuart *vu = vm_vuart(vm);

	/* Set baud rate*/
	divisor = (UART_CLOCK_RATE / BAUD_9600) >> 4U;
	vm->vuart.dll = (uint8_t)divisor;
	vm->vuart.dlh = (uint8_t)(divisor >> 8U);

	vm->vuart.active = false;
	vm->vuart.base = CONFIG_COM_BASE;
	vm->vuart.vm = vm;
	vuart_fifo_init(vu);
	vuart_lock_init(vu);
	vuart_register_io_handler(vm);
}
