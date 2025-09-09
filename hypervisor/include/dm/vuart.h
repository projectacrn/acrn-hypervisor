/*-
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
 * Copyright (c) 2018-2025 Intel Corporation.
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

#ifndef VUART_H
#define VUART_H
#include <types.h>
#include <spinlock.h>
#include <asm/vm_config.h>

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief All APIs to support virtual UART device.
 *
 * This file defines macros, structures and function declarations for emulating virtual UART device.
 */

#define RX_BUF_SIZE		CONFIG_VUART_RX_BUF_SIZE
#define TX_BUF_SIZE		CONFIG_VUART_TX_BUF_SIZE
#define VUART_TIMER_CPU		CONFIG_VUART_TIMER_PCPU
#define INVAILD_VUART_IDX	0xFFU

#define COM1_BASE		0x3F8U
#define COM2_BASE		0x2F8U
#define COM3_BASE		0x3E8U
#define COM4_BASE		0x2E8U
#define INVALID_COM_BASE	0U

#define COM1_IRQ		4U
#define COM2_IRQ		3U
#define COM3_IRQ		6U
#define COM4_IRQ		7U

struct vuart_fifo {
	char *buf;
	uint32_t rindex;	/* index to read from */
	uint32_t windex;	/* index to write to */
	uint32_t num;		/* number of characters in the fifo */
	uint32_t size;		/* size of the fifo */
};

/**
 * @brief Data structure to illustrate a virtual UART device.
 *
 * This structure contains the information of a virtual UART device.
 *
 * @consistency self.vm->vuart[X] == self
 * @alignment N/A
 *
 * @remark N/A
 */
struct acrn_vuart {
	uint8_t data; /**< Data register (R/W). */
	uint8_t ier; /**< Interrupt enable register (R/W). */
	uint8_t lcr; /**< Line control register (R/W). */
	uint8_t mcr; /**< Modem control register (R/W). */
	uint8_t lsr; /**< Line status register (R/W). */
	uint8_t msr; /**< Modem status register (R/W). */
	uint8_t fcr; /**< FIFO control register (W). */
	uint8_t scr; /**< Scratch register (R/W). */
	uint8_t dll; /**< Baudrate divisor latch LSB. */
	uint8_t dlh; /**< Baudrate divisor latch MSB. */

	struct vuart_fifo rxfifo; /**< FIFO queue for received data. */
	struct vuart_fifo txfifo; /**< FIFO queue for transmitted data. */
	uint16_t port_base; /**< Base port address of the virtual UART device. */
	uint32_t irq; /**< IRQ number of the virtual UART device. */
	char vuart_rx_buf[RX_BUF_SIZE]; /**< Buffer for received data. */
	char vuart_tx_buf[TX_BUF_SIZE]; /**< Buffer for transmitted data. */
	bool thre_int_pending; /**< Whether Transmitter Holding Register Empty(THRE) interrupt is pending. */
	bool active; /**< Whether the vuart is active. */
	bool escaping; /**< Whether in escaping sequence, only for console vuarts. */
	struct acrn_vuart *target_vu; /**< Pointer to target vuart */
	struct acrn_vm *vm; /**< Pointer to the VM that owns the virtual UART device. */
	struct pci_vdev *vdev; /**< Pointer to the PCI device, only for a PCI vuart. */
	spinlock_t lock; /**< The spinlock to protect simultaneous access of all elements. */
};

void init_legacy_vuarts(struct acrn_vm *vm, const struct vuart_config *vu_config);
void deinit_legacy_vuarts(struct acrn_vm *vm);
void init_pci_vuart(struct pci_vdev *vdev);
void deinit_pci_vuart(struct pci_vdev *vdev);

void vuart_putchar(struct acrn_vuart *vu, char ch);
char vuart_getchar(struct acrn_vuart *vu);
void vuart_toggle_intr(const struct acrn_vuart *vu);

bool is_vuart_intx(const struct acrn_vm *vm, uint32_t intx_gsi);

uint8_t vuart_read_reg(struct acrn_vuart *vu, uint16_t offset);
void vuart_write_reg(struct acrn_vuart *vu, uint16_t offset, uint8_t value);
#endif /* VUART_H */

/**
 * @}
 */