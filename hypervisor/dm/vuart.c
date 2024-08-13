/*-
 * Copyright (c) 2012 NetApp, Inc.
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
 * Copyright (c) 2018-2024 Intel Corporation.
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
#include <vmcs9900.h>
#include <asm/guest/vm.h>
#include <logmsg.h>

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief Implementation of virtual UART device.
 *
 * This file implements all the APIs to support virtual UART device. It also defines some helper functions to simulate
 * the device that are commonly used in this file.
 */

#define init_vuart_lock(vu)	spinlock_init(&((vu)->lock))
#define obtain_vuart_lock(vu, flags)	spinlock_irqsave_obtain(&((vu)->lock), &(flags))
#define release_vuart_lock(vu, flags)	spinlock_irqrestore_release(&((vu)->lock), (flags))

static inline void reset_fifo(struct vuart_fifo *fifo)
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

static inline bool fifo_isfull(const struct vuart_fifo *fifo)
{
	bool ret = false;
	/* When the FIFO has less than 16 empty bytes, it should be
	 * mask as full. As when the 16550 driver in OS receive the
	 * THRE interrupt, it will directly send 16 bytes without
	 * checking the LSR(THRE) */

	/* Desired value should be 16 bytes, but to improve
	 * fault-tolerant, enlarge 16 to 64. So that even the THRE
	 * interrupt is raised by mistake, only if it less than 4
	 * times, data in FIFO will not be overwritten. */
	if ((fifo->size - fifo->num) < 64U) {
		ret = true;
	}
	return ret;
}

void vuart_putchar(struct acrn_vuart *vu, char ch)
{
	uint64_t rflags;

	obtain_vuart_lock(vu, rflags);
	fifo_putchar(&vu->rxfifo, ch);
	release_vuart_lock(vu, rflags);
}

char vuart_getchar(struct acrn_vuart *vu)
{
	uint64_t rflags;
	char c;

	obtain_vuart_lock(vu, rflags);
	c = fifo_getchar(&vu->txfifo);
	release_vuart_lock(vu, rflags);
	return c;
}

static inline void init_fifo(struct acrn_vuart *vu)
{
	vu->txfifo.buf = vu->vuart_tx_buf;
	vu->rxfifo.buf = vu->vuart_rx_buf;
	vu->txfifo.size = TX_BUF_SIZE;
	vu->rxfifo.size = RX_BUF_SIZE;
	reset_fifo(&(vu->txfifo));
	reset_fifo(&(vu->rxfifo));
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

	if (((vu->lsr & (LSR_OE | LSR_BI)) != 0U) && ((vu->ier & IER_ELSI) != 0U)) {
		ret = IIR_RLS;
	} else if ((fifo_numchars(&vu->rxfifo) > 0U) && ((vu->ier & IER_ERBFI) != 0U)) {
		ret = IIR_RXRDY;
	} else if (vu->thre_int_pending && ((vu->ier & IER_ETBEI) != 0U)) {
		ret = IIR_TXRDY;
	} else if(((vu->msr & MSR_DELTA_MASK) != 0U) && ((vu->ier & IER_EMSC) != 0U)) {
		ret = IIR_MLSC;
	} else {
		ret = IIR_NOPEND;
	}
	return ret;
}

static struct acrn_vuart *find_vuart_by_port(struct acrn_vm *vm, uint16_t offset)
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

static void vuart_trigger_level_intr(const struct acrn_vuart *vu, bool assert)
{
	union ioapic_rte rte;
	uint32_t operation;

	vioapic_get_rte(vu->vm, vu->irq, &rte);

	/* TODO:
	 * Here should assert vuart irq according to vCOM1_IRQ polarity.
	 * The best way is to get the polarity info from ACPI table.
	 * Here we just get the info from vioapic configuration.
	 * based on this, we can still have irq storm during guest
	 * modify the vioapic setting, as it's only for debug uart,
	 * we want to make it as an known issue.
	 */
	if (rte.bits.intr_polarity == IOAPIC_RTE_INTPOL_ALO) {
		operation = assert ? GSI_SET_LOW : GSI_SET_HIGH;
	} else {
		operation = assert ? GSI_SET_HIGH : GSI_SET_LOW;
	}

	vpic_set_irqline(vm_pic(vu->vm), vu->irq, operation);
	vioapic_set_irqline_lock(vu->vm, vu->irq, operation);
}

/*
 * Toggle the COM port's intr pin depending on whether or not we have an
 * interrupt condition to report to the processor.
 */
void vuart_toggle_intr(const struct acrn_vuart *vu)
{
	uint8_t intr_reason;

	intr_reason = vuart_intr_reason(vu);

	if ((vu->vdev != NULL) && (intr_reason != IIR_NOPEND)) {
		/* FIXME: Toggle is for level trigger interrupt, for edge trigger need refine the logic later. */
		trigger_vmcs9900_msix(vu->vdev);
	} else if (intr_reason != IIR_NOPEND) {
		vuart_trigger_level_intr(vu, true);
	} else {
		vuart_trigger_level_intr(vu, false);
	}
}

static bool send_to_target(struct acrn_vuart *vu, uint8_t value_u8)
{
	uint64_t rflags;
	bool ret = false;

	obtain_vuart_lock(vu, rflags);
	if (vu->active) {
		fifo_putchar(&vu->rxfifo, (char)value_u8);
		if (fifo_isfull(&vu->rxfifo)) {
			ret = true;
		}
		vuart_toggle_intr(vu);
	}
	release_vuart_lock(vu, rflags);
	return ret;
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
	uint64_t rflags;

	obtain_vuart_lock(vu, rflags);
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
					reset_fifo(&vu->rxfifo);
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
	release_vuart_lock(vu, rflags);
}

/**
 * @brief Write a value to a register in the virtual UART.
 *
 * This function writes a 8-bit value to a specified register in the virtual UART (vUART). It is a basic function used
 * for port I/O write or MMIO write.
 *
 * - When the following conditions are met, data is sent to the target vUART's RXFIFO:
 *   1) The target vUART exists for the specified vUART, which is used for communication.
 *   2) The accessed register is the THR (Transmitter Holding Register).
 *   3) Loopback mode is not enabled.
 *   4) DLAB (Divisor Latch Access Bit) is not set.
 *   - Additionally, to ensure reliable communication, it raises the THRE interrupt (indicating that more data can be
 *     processed) only if the target vUART's RXFIFO is not full.
 * - If these conditions are not met, the virtual registers specified by the offset are updated according to the 16550
 *   UART specification.
 *
 * @param[inout] vu The virtual UART structure to which the register value is to be written.
 * @param[in] offset The offset of the register within the vUART.
 * @param[in] value_u8 The 8-bit value to be written to the register.
 *
 * @return None
 *
 * @pre vu != NULL
 *
 * @post N/A
 */
void vuart_write_reg(struct acrn_vuart *vu, uint16_t offset, uint8_t value_u8)
{
	struct acrn_vuart *target_vu = NULL;
	uint64_t rflags;

	target_vu = vu->target_vu;

	if (((vu->mcr & MCR_LOOPBACK) == 0U) && ((vu->lcr & LCR_DLAB) == 0U)
		&& (offset == UART16550_THR) && (target_vu != NULL)) {
		if (!send_to_target(target_vu, value_u8)) {
			/* FIFO is not full, raise THRE interrupt */
			obtain_vuart_lock(vu, rflags);
			vu->thre_int_pending = true;
			vuart_toggle_intr(vu);
			release_vuart_lock(vu, rflags);
		}
	} else {
		write_reg(vu, offset, value_u8);
	}
}

/**
 * @brief Write a value to a port in the legacy virtual UART.
 *
 * This function writes a value to the legacy virtual UART (vUART) based on the specified port address. It is used to
 * handle I/O port write operations in the VM by updating the vUART's register. This function is typically called when
 * the vCPU needs to write data to the vUART during emulation of I/O port operations.
 *
 * - Based on the specified port address, it first finds the vUART device within the VM corresponding to the given vCPU.
 * - If the vUART is found, the value is written to the corresponding register. For detailed write operations, refer to
 *   vuart_write_reg().
 * - If the vUART is not found, the write operation is ignored.
 *
 * @param[inout] vcpu A pointer to the vCPU that initiates the write operation.
 * @param[in] offset_arg The port address to write to.
 * @param[in] width The width of the write operation (unused in this function).
 * @param[in] value The value to be written to the register.
 *
 * @return Always returns true.
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 *
 * @post N/A
 */
static bool vuart_write(struct acrn_vcpu *vcpu, uint16_t offset_arg,
			__unused size_t width, uint32_t value)
{
	uint16_t offset = offset_arg;
	struct acrn_vuart *vu = find_vuart_by_port(vcpu->vm, offset);
	uint8_t value_u8 = (uint8_t)value;

	if (vu != NULL) {
		offset -= vu->port_base;
		vuart_write_reg(vu, offset, value_u8);
	}
	return true;
}

static void notify_target(const struct acrn_vuart *vu)
{
	struct acrn_vuart *t_vu;
	uint64_t rflags;

	if (vu != NULL) {
		t_vu = vu->target_vu;
		if ((t_vu != NULL) && !fifo_isfull(&vu->rxfifo)) {
			obtain_vuart_lock(t_vu, rflags);
			t_vu->thre_int_pending = true;
			vuart_toggle_intr(t_vu);
			release_vuart_lock(t_vu, rflags);
		}
	}
}

/**
 * @brief Read a register from the virtual UART.
 *
 * This function returns the 8-bit value of a specified register. It is a basic function used for port I/O read or MMIO
 * read. The registers in the UART will affect each other, such as reading the LSR register will clear some bits in it.
 * This function carefully simulates this behavior.
 *
 * - If the DLAB bit in LCR is set, only the DLL and DLH registers are accessed. Other registers are considered as 0.
 * - Otherwise, according to the UART16550 specification, set and read the corresponding register. For registers greater
 *   than 0x07 (SCR), the value will be 0xFF.
 * - It will toggle the interrupt.
 * - For communication vUART, when the data in FIFO is read out (read from RBR), it will notify the target vUART to send
 *   more data.
 * - Finally, it will return the value of the specified register.
 *
 * @param[inout] vu Pointer to the virtual UART device.
 * @param[in] offset The specified offset of the register to read.
 *
 * @return The value of the specified register.
 *
 * @pre vu != NULL
 *
 * @post N/A
 */
uint8_t vuart_read_reg(struct acrn_vuart *vu, uint16_t offset)
{
	struct acrn_vuart *t_vu;
	uint8_t iir, reg = 0U, intr_reason;
	uint64_t rflags;

	t_vu = vu->target_vu;
	obtain_vuart_lock(vu, rflags);
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
			if (t_vu != NULL) {
				if (!fifo_isfull(&t_vu->rxfifo)) {
					vu->lsr |= LSR_TEMT | LSR_THRE;
				}
			} else {
				vu->lsr |= LSR_TEMT | LSR_THRE;
			}
			/* Check for new receive data */
			if (fifo_numchars(&vu->rxfifo) > 0U) {
				vu->lsr |= LSR_DR;
			} else {
				vu->lsr &= ~LSR_DR;
			}
			reg = vu->lsr;
			/* The LSR_OE bit is cleared on LSR read */
			vu->lsr &= ~(LSR_OE | LSR_BI);
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
	release_vuart_lock(vu, rflags);

	/* For commnunication vuart, when the data in FIFO is read out, should
	 * notify the target vuart to send more data. */
	if (offset == UART16550_RBR) {
		notify_target(vu);
	}

	return reg;
}

/**
 * @brief Read a value from a port in the legacy virtual UART.
 *
 * This function reads a value from the legacy virtual UART (vUART) based on the specified port address. It is used to
 * handle I/O port read operations in the VM by retrieving the value from the vUART's registers. This function is
 * typically called when the vCPU needs to read data from the vUART during emulation of I/O port operations.
 *
 * - Based on the specified port address, it first finds the vUART device within the VM corresponding to the given vCPU.
 * - If the vUART is found, the value is read from the corresponding virtual register and stored in the vCPU's PIO
 *   request. For detailed read operations, refer to vuart_read_reg().
 * - If the vUART is not found, the vCPU's PIO request remains unchanged.
 *
 * @param[inout] vcpu A pointer to the vCPU that initiates the read operation.
 * @param[in] offset_arg The port address to read from.
 * @param[in] width The width of the read operation (unused in this function).
 *
 * @return Always returns true.
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 *
 * @post N/A
 */
static bool vuart_read(struct acrn_vcpu *vcpu, uint16_t offset_arg, __unused size_t width)
{
	uint16_t offset = offset_arg;
	struct acrn_vuart *vu = find_vuart_by_port(vcpu->vm, offset);
	struct acrn_pio_request *pio_req = &vcpu->req.reqs.pio_request;

	if (vu != NULL) {
		offset -= vu->port_base;
		pio_req->value = (uint32_t)vuart_read_reg(vu, offset);
	}

	return true;
}

/*
 * @pre: vuart_idx < MAX_VUART_NUM_PER_VM
 */
static bool vuart_register_io_handler(struct acrn_vm *vm, uint16_t port_base, uint32_t vuart_idx)
{
	bool ret = true;

	struct vm_io_range range = {
		.base = port_base,
		.len = 8U
	};
	if (vuart_idx < MAX_VUART_NUM_PER_VM) {
		register_pio_emulation_handler(vm, UART_PIO_IDX0 + vuart_idx, &range, vuart_read, vuart_write);
	} else {
		printf("Not support vuart index %d, will not register \n", vuart_idx);
		ret = false;
	}
	return ret;
}

static void setup_vuart(struct acrn_vm *vm, uint16_t vuart_idx)
{
	uint32_t divisor;
	struct acrn_vuart *vu = &vm->vuart[vuart_idx];

	/* Set baud rate*/
	divisor = (UART_CLOCK_RATE / BAUD_115200) >> 4U;
	vu->dll = (uint8_t)divisor;
	vu->dlh = (uint8_t)(divisor >> 8U);
	vu->vm = vm;
	init_fifo(vu);
	init_vuart_lock(vu);
	vu->thre_int_pending = true;
	vu->ier = 0U;
	vuart_toggle_intr(vu);
	vu->target_vu = NULL;
}

static struct acrn_vuart *find_active_target_vuart(const struct vuart_config *vu_config)
{
	struct acrn_vm *target_vm = NULL;
	struct acrn_vuart *target_vu = NULL, *ret_vu = NULL;
	uint16_t target_vmid, target_vuid;

	target_vmid = vu_config->t_vuart.vm_id;
	target_vuid = vu_config->t_vuart.vuart_id;

	if ((target_vmid < CONFIG_MAX_VM_NUM) && (target_vuid < MAX_VUART_NUM_PER_VM)) {
		target_vm = get_vm_from_vmid(target_vmid);
		target_vu = &target_vm->vuart[target_vuid];

		if ((target_vu != NULL) && (target_vu->active)) {
			ret_vu = target_vu;
		}
	}

	return ret_vu;
}

static void vuart_setup_connection(struct acrn_vm *vm,
		const struct vuart_config *vu_config, uint16_t vuart_idx)
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

static void vuart_deinit_connection(struct acrn_vuart *vu)
{
	struct acrn_vuart *t_vu = vu->target_vu;

	t_vu->target_vu = NULL;
	vu->target_vu = NULL;
}

/**
 * @brief Check if any of the vUARTs in the given VM uses the specified INTx interrupt.
 *
 * This function checks whether the INTx interrupt is already used by vUART. It's usually used when the hypervisor
 * tries to add a new INTx remapping.
 *
 * It will check all the vUARTs in the VM to see if any of them uses the specified INTx interrupt. If any of the vUARTs
 * is active and using the specified INTx interrupt, the function will return true. Otherwise, it will return false.
 *
 * @param[in] vm Pointer to the VM.
 * @param[in] intx_gsi The INTx interrupt number to check against the vUART configurations.
 *
 * @return A boolean value indicating if any of the vUARTs in the VM uses the specified INTx interrupt.
 *
 * @retval true If any of the vUARTs in the VM uses the specified INTx interrupt.
 * @retval false If none of the vUARTs in the VM uses the specified INTx interrupt.
 *
 * @pre vm != NULL
 *
 * @post N/A
 */
bool is_vuart_intx(const struct acrn_vm *vm, uint32_t intx_gsi)
{
	uint8_t i;
	bool ret = false;

	for (i = 0U; i < MAX_VUART_NUM_PER_VM; i++) {
		if ((vm->vuart[i].active) && (vm->vuart[i].irq == intx_gsi)) {
			ret = true;
		}
	}
	return ret;
}

/**
 * @brief Initialize legacy virtual UART devices.
 *
 * This function initializes the legacy virtual UARTs (vUARTs) in hypervisor. A vUART is emulated as a 16550 UART device
 * and can exchange data between the hypervisor and a VM or between two VMs. It sets up the necessary configurations and
 * resources required for the vUARTs to function correctly. This function is usually called during VM creation.
 *
 * A VM can have several vUARTs (including legacy and PCI vUARTs). The first vUART is used for the VM console, and the
 * rest are used for communication between VMs. Every legacy vUART device defined in the vUART configuration list is
 * initialized and configured. It will also register port I/O handlers for every vUART device and set up the connection
 * between vUARTs for communication.
 *
 * @param[inout] vm Pointer to the VM that owns the vUART devices.
 * @param[in] vu_config Pointer to the vUART configuration structure list that contains the configuration information.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre vu_config != NULL
 *
 * @post N/A
 */
void init_legacy_vuarts(struct acrn_vm *vm, const struct vuart_config *vu_config)
{
	uint8_t i;
	struct acrn_vuart *vu;

	for (i = 0U; i < MAX_VUART_NUM_PER_VM; i++) {
		if ((vu_config[i].type == VUART_LEGACY_PIO) &&
				(vu_config[i].addr.port_base != INVALID_COM_BASE)) {
			vu = &vm->vuart[i];
			setup_vuart(vm, i);
			vu->port_base = vu_config[i].addr.port_base;
			vu->irq = vu_config[i].irq;
			if (vuart_register_io_handler(vm, vu->port_base, i) != 0U) {
				vu->active = true;
				vu->escaping = false;
			}
			/*
			 * The first vuart is used for VM console.
			 * The rest of vuarts are used for connection.
			 */
			if (i != 0U) {
				vuart_setup_connection(vm, &vu_config[i], i);
			}
		}
	}
}

/**
 * @brief Deinitialize legacy virtual UART devices.
 *
 * This function deinitializes the legacy virtual UARTs (vUARTs) in hypervisor. It cleans up the resources and
 * configurations that were set up for the vUARTs during initialization. This function should be called when the vUARTs
 * are no longer needed, such as when a VM is being destroyed.
 *
 * The function will deinitialize all vUARTs associated with the given VM. It will set the active flag to false and
 * remove the connection between vUARTs for communication.
 *
 * @param[inout] vm Pointer to the VM that owns the vUART devices.
 *
 * @return None
 *
 * @pre vm != NULL
 *
 * @post N/A
 */
void deinit_legacy_vuarts(struct acrn_vm *vm)
{
	uint8_t i;

	for (i = 0U; i < MAX_VUART_NUM_PER_VM; i++) {
		if (vm->vuart[i].port_base != INVALID_COM_BASE) {
			vm->vuart[i].active = false;
			vm->vuart[i].escaping = false;
			if (vm->vuart[i].target_vu != NULL) {
				vuart_deinit_connection(&vm->vuart[i]);
			}
		}
	}
}

/**
 * @brief Initialize a PCI virtual UART device.
 *
 * This function initializes a PCI-based virtual UART (vUART) in hypervisor. A MCS9900 controller is emulated as a PCI
 * device, and the vUART device is a part of the MCS9900 controller. This function is usually called during the VM
 * creation and after the MCS9900 controller device is initialized.
 *
 * A VM can have several vUARTs (including legacy and PCI vUARTs). The first vUART is used for the VM console, and the
 * rest are used for communication between VMs. The PCI vUARTs are only used for communication between VMs for now. It
 * will initialize the vUART associated with the given PCI device and set up the connection between vUARTs for
 * communication.
 *
 * @param[inout] vdev Pointer to the PCI device that owns the vUART.
 *
 * @return None
 *
 * @pre vdev != NULL
 *
 * @post N/A
 */
void init_pci_vuart(struct pci_vdev *vdev)
{
	struct acrn_vuart *vu = vdev->priv_data;
	struct acrn_vm_pci_dev_config *pci_cfg = vdev->pci_dev_config;
	uint16_t idx = pci_cfg->vuart_idx;
	struct acrn_vm *vm = container_of(vdev->vpci, struct acrn_vm, vpci);
	struct acrn_vm_config *vm_cfg = get_vm_config(vm->vm_id);

	setup_vuart(vm, idx);
	vu->vdev = vdev;
	vm_cfg->vuart[idx].type = VUART_PCI;
	vm_cfg->vuart[idx].t_vuart.vm_id = pci_cfg->t_vuart.vm_id;
	vm_cfg->vuart[idx].t_vuart.vuart_id = pci_cfg->t_vuart.vuart_id;

	vu->active = true;
	vu->escaping = false;
	if (pci_cfg->vuart_idx != 0U) {
		vuart_setup_connection(vm, &vm_cfg->vuart[idx], idx);
	}

}

/**
 * @brief Deinitialize a PCI virtual UART device.
 *
 * This function deinitializes a PCI virtual UART (vUART) in hypervisor. It cleans up the resources and configurations
 * that were set up for the vUART during initialization. This function should be called when the vUART is no longer
 * needed, such as when a VM is being destroyed.
 *
 * The function will deinitialize the vUART associated with the given PCI device. It will set the active flag to false
 * and remove the connection between vUARTs for communication.
 *
 * @param[inout] vdev Pointer to the PCI device that owns the vUART.
 *
 * @return None
 *
 * @pre vdev != NULL
 *
 * @post N/A
 */
void deinit_pci_vuart(struct pci_vdev *vdev)
{
	struct acrn_vuart *vu = vdev->priv_data;

	vu->active = false;
	vu->escaping = false;
	if (vu->target_vu != NULL) {
		vuart_deinit_connection(vu);
	}
}

/**
 * @}
 */