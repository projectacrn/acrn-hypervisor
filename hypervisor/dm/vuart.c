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

#include <types.h>
#include <pci.h>
#include <uart16550.h>
#include <console.h>
#include <vuart.h>
#include <vm.h>
#include <logmsg.h>

#define vuart_lock_init(vu)	spinlock_init(&((vu)->lock))
#define vuart_lock(vu)		spinlock_obtain(&((vu)->lock))
#define vuart_unlock(vu)	spinlock_release(&((vu)->lock))

static inline void fifo_reset(struct vuart_fifo *fifo)
{
	fifo->rindex = 0U;
	fifo->windex = 0U;
	fifo->num = 0U;
}

static inline void fifo_putchar(struct vuart_fifo *fifo, char ch)
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

static inline char fifo_getchar(struct vuart_fifo *fifo)
{
	char c = -1;

	if (fifo->num > 0U) {
		c = fifo->buf[fifo->rindex];
		fifo->rindex = (fifo->rindex + 1U) % fifo->size;
		fifo->num--;
	}
	return c;
}

static inline uint32_t fifo_numchars(const struct vuart_fifo *fifo)
{
	return fifo->num;
}

void vuart_putchar(struct acrn_vuart *vu, char ch)
{
	vuart_lock(vu);
	fifo_putchar(&vu->rxfifo, ch);
	vuart_unlock(vu);
}

char vuart_getchar(struct acrn_vuart *vu)
{
	char c;

	vuart_lock(vu);
	c = fifo_getchar(&vu->txfifo);
	vuart_unlock(vu);
	return c;
}

static inline void vuart_fifo_init(struct acrn_vuart *vu)
{
	vu->txfifo.buf = vu->vuart_tx_buf;
	vu->rxfifo.buf = vu->vuart_rx_buf;
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
	uint8_t ret;

	if (((vu->lsr & LSR_OE) != 0U) && ((vu->ier & IER_ELSI) != 0U)) {
		ret = IIR_RLS;
	} else if ((fifo_numchars(&vu->rxfifo) > 0U) && ((vu->ier & IER_ERBFI) != 0U)) {
		ret = IIR_RXTOUT;
	} else if (vu->thre_int_pending && ((vu->ier & IER_ETBEI) != 0U)) {
		ret = IIR_TXRDY;
	} else if(((vu->msr & MSR_DELTA_MASK) != 0U) && ((vu->ier & IER_EMSC) != 0U)) {
		ret = IIR_MLSC;
	} else {
		ret = IIR_NOPEND;
	}
	return ret;
}

struct acrn_vuart *find_vuart_by_port(struct acrn_vm *vm, uint16_t offset)
{
	uint8_t i;
	struct acrn_vuart *vu, *ret_vu = NULL;

	/* TODO: support pci vuart find */
	for (i = 0U; i < MAX_VUART_NUM_PER_VM; i++) {
		vu = &vm->vuart[i];
		if ((vu->active) && (vu->port_base == (offset & ~0x7U))) {
			ret_vu = vu;
			break;
		}
	}
	return ret_vu;
}

/*
 * Toggle the COM port's intr pin depending on whether or not we have an
 * interrupt condition to report to the processor.
 */
void vuart_toggle_intr(const struct acrn_vuart *vu)
{
	uint8_t intr_reason;
	union ioapic_rte rte;
	uint32_t operation;

	intr_reason = vuart_intr_reason(vu);
	vioapic_get_rte(vu->vm, vu->irq, &rte);

	/* TODO:
	 * Here should assert vuart irq according to CONFIG_COM_IRQ polarity.
	 * The best way is to get the polarity info from ACIP table.
	 * Here we just get the info from vioapic configuration.
	 * based on this, we can still have irq storm during guest
	 * modify the vioapic setting, as it's only for debug uart,
	 * we want to make it as an known issue.
	 */
	if (rte.bits.intr_polarity == IOAPIC_RTE_INTPOL_ALO) {
		operation = (intr_reason != IIR_NOPEND) ? GSI_SET_LOW : GSI_SET_HIGH;
	} else {
		operation = (intr_reason != IIR_NOPEND) ? GSI_SET_HIGH : GSI_SET_LOW;
	}

	vpic_set_irqline(vm_pic(vu->vm), vu->irq, operation);
	vioapic_set_irqline_lock(vu->vm, vu->irq, operation);
}

static void send_to_target(struct acrn_vuart *vu, uint8_t value_u8)
{
	vuart_lock(vu);
	if (vu->active) {
		fifo_putchar(&vu->rxfifo, (char)value_u8);
		vu->thre_int_pending = true;
		vuart_toggle_intr(vu);
	}
	vuart_unlock(vu);
}

static uint8_t get_modem_status(uint8_t mcr)
{
	uint8_t msr;

	if ((mcr & MCR_LOOPBACK) != 0U) {
		/*
		 * In the loopback mode certain bits from the MCR are
		 * reflected back into MSR.
		 */
		msr = 0U;
		if ((mcr & MCR_RTS) != 0U) {
			msr |= MSR_CTS;
		}
		if ((mcr & MCR_DTR) != 0U) {
			msr |= MSR_DSR;
		}
		if ((mcr & MCR_OUT1) != 0U) {
			msr |= MSR_RI;
		}
		if ((mcr & MCR_OUT2) != 0U) {
			msr |= MSR_DCD;
		}
	} else {
		/*
		 * Always assert DCD and DSR so tty open doesn't block
		 * even if CLOCAL is turned off.
		 */
		msr = MSR_DCD | MSR_DSR;
	}
	return msr;
}

static uint8_t update_modem_status(uint8_t new_msr, uint8_t old_msr)
{
	uint8_t update_msr = old_msr;
	/*
	 * Detect if there has been any change between the
	 * previous and the new value of MSR. If there is
	 * then assert the appropriate MSR delta bit.
	 */
	if (((new_msr & MSR_CTS) ^ (old_msr & MSR_CTS)) != 0U) {
		update_msr |= MSR_DCTS;
	}
	if (((new_msr & MSR_DSR) ^ (old_msr & MSR_DSR)) != 0U) {
		update_msr |= MSR_DDSR;
	}
	if (((new_msr & MSR_DCD) ^ (old_msr & MSR_DCD)) != 0U) {
		update_msr |= MSR_DDCD;
	}
	if (((new_msr & MSR_RI) == 0U) && ((old_msr & MSR_RI) != 0U)) {
		update_msr |= MSR_TERI;
	}
	update_msr &= MSR_DELTA_MASK;
	update_msr |= new_msr;

	return update_msr;
}

/*
 * @pre: vu != NULL
 */
static void write_reg(struct acrn_vuart *vu, uint16_t reg, uint8_t value_u8)
{
	uint8_t msr;

	vuart_lock(vu);
	/*
	 * Take care of the special case DLAB accesses first
	 */
	if (((vu->lcr & LCR_DLAB) != 0U) && (reg == UART16550_DLL)) {
		vu->dll = value_u8;
	} else if (((vu->lcr & LCR_DLAB) != 0U) && (reg == UART16550_DLM)) {
		vu->dlh = value_u8;
	} else {
		switch (reg) {
		case UART16550_THR:
			if ((vu->mcr & MCR_LOOPBACK) != 0U) {
				fifo_putchar(&vu->rxfifo, (char)value_u8);
				vu->lsr |= LSR_OE;
			} else {
				fifo_putchar(&vu->txfifo, (char)value_u8);
			}
			vu->thre_int_pending = true;
			break;
		case UART16550_IER:
			if (((vu->ier & IER_ETBEI) == 0U) && ((value_u8 & IER_ETBEI) != 0U)) {
				vu->thre_int_pending = true;
			}
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
				vu->fcr = value_u8 & (FCR_FIFOE | FCR_DMA | FCR_RX_MASK);
			}
			break;
		case UART16550_LCR:
			vu->lcr = value_u8;
			break;
		case UART16550_MCR:
			/* Apply mask so that bits 5-7 are 0 */
			vu->mcr = value_u8 & 0x1FU;
			msr = get_modem_status(vu->mcr);
			/*
			 * Update the value of MSR while retaining the delta
			 * bits.
			 */
			vu->msr = update_modem_status(msr, vu->msr);
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
			 * For the reg that is not handled (either a read-only
			 * register or an invalid register), ignore the write to it.
			 * Gracefully return if prior case clauses have not been met.
			 */
			break;
		}
	}
	vuart_toggle_intr(vu);
	vuart_unlock(vu);
}

static bool vuart_write(struct acrn_vm *vm, uint16_t offset_arg,
			__unused size_t width, uint32_t value)
{
	uint16_t offset = offset_arg;
	struct acrn_vuart *vu = find_vuart_by_port(vm, offset);
	uint8_t value_u8 = (uint8_t)value;
	struct acrn_vuart *target_vu = NULL;

	if (vu != NULL) {
		offset -= vu->port_base;
		target_vu = vu->target_vu;

		if (((vu->mcr & MCR_LOOPBACK) == 0U) &&
			(offset == UART16550_THR) && (target_vu != NULL)) {
			send_to_target(target_vu, value_u8);
		} else {
			write_reg(vu, offset, value_u8);
		}
	}
	return true;
}

static bool vuart_read(struct acrn_vm *vm, struct acrn_vcpu *vcpu, uint16_t offset_arg,
			__unused size_t width)
{
	uint16_t offset = offset_arg;
	uint8_t iir, reg, intr_reason;
	struct acrn_vuart *vu = find_vuart_by_port(vm, offset);
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	if (vu != NULL) {
		offset -= vu->port_base;
		vuart_lock(vu);
		/*
		 * Take care of the special case DLAB accesses first
		 */
		if ((vu->lcr & LCR_DLAB) != 0U) {
			if (offset == UART16550_DLL) {
				reg = vu->dll;
			} else if (offset == UART16550_DLM) {
				reg = vu->dlh;
			} else {
				reg = 0U;
			}
		} else {
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
				/*
				 * MSR delta bits are cleared on read
				 */
				reg = vu->msr;
				vu->msr &= ~MSR_DELTA_MASK;
				break;
			case UART16550_SCR:
				reg = vu->scr;
				break;
			default:
				reg = 0xFFU;
				break;
			}
		}
		vuart_toggle_intr(vu);
		pio_req->value = (uint32_t)reg;
		vuart_unlock(vu);
	}

	return true;
}

/*
 * @pre: vuart_idx = 0 or 1
 */
static bool vuart_register_io_handler(struct acrn_vm *vm, uint16_t port_base, uint32_t vuart_idx)
{
	uint32_t pio_idx;
	bool ret = true;

	struct vm_io_range range = {
		.flags = IO_ATTR_RW,
		.base = port_base,
		.len = 8U
	};
	switch (vuart_idx) {
	case 0U:
		pio_idx = UART_PIO_IDX0;
		break;
	case 1U:
		pio_idx = UART_PIO_IDX1;
		break;
	default:
		printf("Not support vuart index %d, will not register \n");
		ret = false;
	}
	if (ret != 0U) {
		register_pio_emulation_handler(vm, pio_idx, &range, vuart_read, vuart_write);
	}
	return ret;
}

static void vuart_setup(struct acrn_vm *vm,
		struct vuart_config *vu_config, uint16_t vuart_idx)
{
	uint32_t divisor;
	struct acrn_vuart *vu = &vm->vuart[vuart_idx];

	/* Set baud rate*/
	divisor = (UART_CLOCK_RATE / BAUD_115200) >> 4U;
	vu->dll = (uint8_t)divisor;
	vu->dlh = (uint8_t)(divisor >> 8U);
	vu->vm = vm;
	vuart_fifo_init(vu);
	vuart_lock_init(vu);
	vu->thre_int_pending = true;
	vu->ier = 0U;
	vuart_toggle_intr(vu);
	vu->target_vu = NULL;
	if (vu_config->type == VUART_LEGACY_PIO) {
		vu->port_base = vu_config->addr.port_base;
		vu->irq = vu_config->irq;
		if (vuart_register_io_handler(vm, vu->port_base, vuart_idx) != 0U) {
			vu->active = true;
		}
	} else {
		/*TODO: add pci vuart support here*/
		printf("PCI vuart is not support\n");
	}
}

static struct acrn_vuart *find_active_target_vuart(struct vuart_config *vu_config)
{
	struct acrn_vm *target_vm = NULL;
	struct acrn_vuart *target_vu = NULL, *ret_vu = NULL;
	uint16_t target_vmid, target_vuid;

	target_vmid = vu_config->t_vuart.vm_id;
	target_vuid = vu_config->t_vuart.vuart_id;
	if (target_vmid < CONFIG_MAX_VM_NUM)
		target_vm = get_vm_from_vmid(target_vmid);

	if (target_vuid < MAX_VUART_NUM_PER_VM)
		target_vu = &target_vm->vuart[target_vuid];

	if ((target_vu != NULL) && (target_vu->active)) {
		ret_vu = target_vu;
	}
	return ret_vu;
}

static void vuart_setup_connection(struct acrn_vm *vm,
		struct vuart_config *vu_config, uint16_t vuart_idx)
{
	struct acrn_vuart *vu, *t_vu;

	vu = &vm->vuart[vuart_idx];
	if (vu->active) {
		t_vu = find_active_target_vuart(vu_config);
		if ((t_vu != NULL) && (t_vu->target_vu == NULL)) {
			vu->target_vu = t_vu;
			t_vu->target_vu = vu;
		}
	}
}

void vuart_deinit_connect(struct acrn_vuart *vu)
{
	struct acrn_vuart *t_vu = vu->target_vu;

	t_vu->target_vu = NULL;
	vu->target_vu = NULL;
}

bool is_vuart_intx(struct acrn_vm *vm, uint32_t intx_pin)
{
	uint8_t i;
	bool ret = false;

	for (i = 0U; i < MAX_VUART_NUM_PER_VM; i++) {
		if ((vm->vuart[i].active) && (vm->vuart[i].irq == intx_pin)) {
			ret = true;
		}
	}
	return ret;
}

void vuart_init(struct acrn_vm *vm, struct vuart_config *vu_config)
{
	uint8_t i;

	for (i = 0U; i < MAX_VUART_NUM_PER_VM; i++) {
		vm->vuart[i].active = false;
		/* This vuart is not exist */
		if ((vu_config[i].type == VUART_LEGACY_PIO) &&
				(vu_config[i].addr.port_base == INVALID_COM_BASE)) {
			continue;
		}
		vuart_setup(vm, &vu_config[i], i);
		/*
		 * The first vuart is used for VM console.
		 * The rest of vuarts are used for connection.
		 */
		if (i != 0U) {
			vuart_setup_connection(vm, &vu_config[i], i);
		}
	}
}

void vuart_deinit(struct acrn_vm *vm)
{
	uint8_t i;

	for (i = 0U; i < MAX_VUART_NUM_PER_VM; i++) {
		vm->vuart[i].active = false;
		if (vm->vuart[i].target_vu != NULL) {
			vuart_deinit_connect(&vm->vuart[i]);
		}
	}
}
