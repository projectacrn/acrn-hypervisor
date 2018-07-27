/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include "uart16550.h"

#if defined(CONFIG_SERIAL_PIO_BASE)
static int serial_port_mapped = 1;
static int uart_enabled = 1;
#define UART_BASE_ADDRESS		CONFIG_SERIAL_PIO_BASE
#elif defined(CONFIG_SERIAL_MMIO_BASE)
static int serial_port_mapped;
static int uart_enabled = 1;
#define UART_BASE_ADDRESS		CONFIG_SERIAL_MMIO_BASE
#else
#warning  "no uart base configure, please check!"
static int serial_port_mapped;
static int uart_enabled;
#define UART_BASE_ADDRESS		0
#endif

typedef uint32_t uart_reg_t;

static uint64_t uart_base_address;

static spinlock_t uart_rx_lock;
static spinlock_t uart_tx_lock;

static inline uint32_t uart16550_read_reg(uint64_t base, uint32_t reg_idx)
{
	if (serial_port_mapped != 0) {
		return io_read_byte((uint16_t)base + reg_idx);
	} else {
		return mmio_read_long((void*)((uint32_t*)HPA2HVA(base) + reg_idx));
	}
}

static inline void uart16550_write_reg(uint64_t base,
	uint32_t val, uint32_t reg_idx)
{
	if (serial_port_mapped != 0) {
		io_write_byte(val, (uint16_t)base + reg_idx);
	} else {
		mmio_write_long(val, (void*)((uint32_t*)HPA2HVA(base) + reg_idx));
	}
}

static void uart16550_calc_baud_div(uint32_t ref_freq,
				uint32_t *baud_div_ptr, uint32_t baud_rate_arg)
{
	uint32_t baud_rate = baud_rate_arg;
	uint32_t baud_multiplier = baud_rate < BAUD_460800 ? 16 : 13;

	if (baud_rate == 0) {
		baud_rate = BAUD_115200;
	}
	*baud_div_ptr = ref_freq / (baud_multiplier * baud_rate);
}

static void uart16550_set_baud_rate(uint32_t baud_rate)
{
	uint32_t baud_div, duart_clock = UART_CLOCK_RATE;
	uart_reg_t temp_reg;

	/* Calculate baud divisor */
	uart16550_calc_baud_div(duart_clock, &baud_div, baud_rate);

	/* Enable DLL and DLM registers for setting the Divisor */
	temp_reg = uart16550_read_reg(uart_base_address, UART16550_LCR);
	temp_reg |= LCR_DLAB;
	uart16550_write_reg(uart_base_address, temp_reg, UART16550_LCR);

	/* Write the appropriate divisor value */
	uart16550_write_reg(uart_base_address,
			((baud_div >> 8) & 0xFFU), UART16550_DLM);
	uart16550_write_reg(uart_base_address,
			(baud_div & 0xFFU), UART16550_DLL);

	/* Disable DLL and DLM registers */
	temp_reg &= ~LCR_DLAB;
	uart16550_write_reg(uart_base_address, temp_reg, UART16550_LCR);
}

void uart16550_init(void)
{
	if (uart_base_address == 0) {
		uart_base_address = UART_BASE_ADDRESS;
	}
	spinlock_init(&uart_rx_lock);
	spinlock_init(&uart_tx_lock);
	/* Enable TX and RX FIFOs */
	uart16550_write_reg(uart_base_address,
			FCR_FIFOE | FCR_RFR | FCR_TFR, UART16550_FCR);

	/* Set-up data bits / parity / stop bits. */
	uart16550_write_reg(uart_base_address,
			(LCR_WL8 | LCR_NB_STOP_BITS_1 | LCR_PARITY_NONE),
			UART16550_LCR);

	/* Disable interrupts (we use polling) */
	uart16550_write_reg(uart_base_address,
			UART_IER_DISABLE_ALL, UART16550_IER);

	/* Set baud rate */
	uart16550_set_baud_rate(BAUD_115200);

	/* Data terminal ready + Request to send */
	uart16550_write_reg(uart_base_address,
			MCR_RTS | MCR_DTR, UART16550_MCR);
}

char uart16550_getc(void)
{
	char ret = -1;

	spinlock_obtain(&uart_rx_lock);

	/* If a character has been received, read it */
	if ((uart16550_read_reg(uart_base_address, UART16550_LSR) & LSR_DR)
			== LSR_DR) {
		/* Read a character */
		ret = uart16550_read_reg(uart_base_address, UART16550_RBR);

	}
	spinlock_release(&uart_rx_lock);
	return ret;
}

static void uart16550_putc(const char *buf)
{
	uint32_t reg;
	/* Ensure there are no further Transmit buffer write requests */
	do {
		reg = uart16550_read_reg(uart_base_address, UART16550_LSR);
	} while ((reg & LSR_THRE) == 0U || (reg & LSR_TEMT) == 0U);

	/* Transmit the character. */
	uart16550_write_reg(uart_base_address, *buf, UART16550_THR);
}

int uart16550_puts(const char *buf, uint32_t len)
{
	uint32_t i;

	spinlock_obtain(&uart_tx_lock);
	for (i = 0U; i < len; i++) {
		/* Transmit character */
		uart16550_putc(buf);
		if (*buf == '\n') {
			/* Append '\r', no need change the len */
			uart16550_putc("\r");
		}
		buf++;
	}
	spinlock_release(&uart_tx_lock);
	return (int)len;
}

void uart16550_set_property(int enabled, int port_mapped, uint64_t base_addr)
{
	uart_enabled = enabled;
	serial_port_mapped = port_mapped;
	uart_base_address = base_addr;
}
