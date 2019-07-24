/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include  <types.h>
#include <spinlock.h>
#include <pci.h>
#include <pgtable.h>
#include <uart16550.h>
#include <io.h>
#include <mmu.h>

#define MAX_BDF_LEN 8

struct console_uart {
	bool enabled;
	bool serial_port_mapped;
	union {
		uint16_t port_address;
		void *mmio_base_vaddr;
	};
	spinlock_t rx_lock;
	spinlock_t tx_lock;
};

#if defined(CONFIG_SERIAL_PIO_BASE)
static struct console_uart uart = {
	.enabled = true,
	.serial_port_mapped = true,
	.port_address = CONFIG_SERIAL_PIO_BASE,
};
static char pci_bdf_info[MAX_BDF_LEN + 1U];
#elif defined(CONFIG_SERIAL_PCI_BDF)
static struct console_uart uart = {
	.enabled = true,
};
static char pci_bdf_info[MAX_BDF_LEN + 1U] = CONFIG_SERIAL_PCI_BDF;
#else
static struct console_uart uart;
static char pci_bdf_info[MAX_BDF_LEN + 1U];
#endif

typedef uint32_t uart_reg_t;
static union pci_bdf serial_pci_bdf;

/* PCI BDF must follow format: bus:dev.func, for example 0:18.2 */
static uint16_t get_pci_bdf_value(char *bdf)
{
	char *pos;
	char *start = bdf;
	char dst[3][4];
	uint64_t value= 0UL;
	
	pos = strchr(start, ':');
	if (pos != NULL) {
		strncpy_s(dst[0], 3, start, pos -start);
		start = pos + 1;

		pos = strchr(start, '.');
		if (pos != NULL) {
			strncpy_s(dst[1], 3, start, pos -start);
			start = pos + 1;

			strncpy_s(dst[2], 2, start, 1);
			value= (strtoul_hex(dst[0]) << 8) | (strtoul_hex(dst[1]) << 3) | strtoul_hex(dst[2]);
		}
	}

	return (uint16_t)value;
}

/**
 * @pre uart->enabled == true
 */
static inline uint32_t uart16550_read_reg(struct console_uart uart, uint16_t reg_idx)
{
	if (uart.serial_port_mapped) {
		return pio_read8(uart.port_address + reg_idx);
	} else {
		return mmio_read32((uint32_t *)uart.mmio_base_vaddr + reg_idx);
	}
}

/**
 * @pre uart->enabled == true
 */
static inline void uart16550_write_reg(struct console_uart uart, uint32_t val, uint16_t reg_idx)
{
	if (uart.serial_port_mapped) {
		pio_write8(val, uart.port_address + reg_idx);
	} else {
		mmio_write32(val, (uint32_t *)uart.mmio_base_vaddr + reg_idx);
	}
}

static void uart16550_calc_baud_div(uint32_t ref_freq, uint32_t *baud_div_ptr, uint32_t baud_rate_arg)
{
	uint32_t baud_rate = baud_rate_arg;
	uint32_t baud_multiplier = baud_rate < BAUD_460800 ? 16U : 13U;

	if (baud_rate == 0U) {
		baud_rate = BAUD_115200;
	}
	*baud_div_ptr = ref_freq / (baud_multiplier * baud_rate);
}

/**
 * @pre uart->enabled == true
 */
static void uart16550_set_baud_rate(uint32_t baud_rate)
{
	uint32_t baud_div, duart_clock = UART_CLOCK_RATE;
	uart_reg_t temp_reg;

	/* Calculate baud divisor */
	uart16550_calc_baud_div(duart_clock, &baud_div, baud_rate);

	/* Enable DLL and DLM registers for setting the Divisor */
	temp_reg = uart16550_read_reg(uart, UART16550_LCR);
	temp_reg |= LCR_DLAB;
	uart16550_write_reg(uart, temp_reg, UART16550_LCR);

	/* Write the appropriate divisor value */
	uart16550_write_reg(uart, ((baud_div >> 8U) & 0xFFU), UART16550_DLM);
	uart16550_write_reg(uart, (baud_div & 0xFFU), UART16550_DLL);

	/* Disable DLL and DLM registers */
	temp_reg &= ~LCR_DLAB;
	uart16550_write_reg(uart, temp_reg, UART16550_LCR);
}

void uart16550_init(void)
{
	if (!uart.enabled) {
		return;
	}

	/* if configure serial PCI BDF, get its base MMIO address */
	if (!uart.serial_port_mapped) {
		serial_pci_bdf.value = get_pci_bdf_value(pci_bdf_info);
		uart.mmio_base_vaddr =
			hpa2hva(pci_pdev_read_cfg(serial_pci_bdf, pci_bar_offset(0), 4U) & PCIM_BAR_MEM_BASE);
	}

	if (!uart.serial_port_mapped) {
		hv_access_memory_region_update((uint64_t)uart.mmio_base_vaddr, PDE_SIZE);
	}

	spinlock_init(&uart.rx_lock);
	spinlock_init(&uart.tx_lock);
	/* Enable TX and RX FIFOs */
	uart16550_write_reg(uart, FCR_FIFOE | FCR_RFR | FCR_TFR, UART16550_FCR);

	/* Set-up data bits / parity / stop bits. */
	uart16550_write_reg(uart, (LCR_WL8 | LCR_NB_STOP_BITS_1 | LCR_PARITY_NONE), UART16550_LCR);

	/* Disable interrupts (we use polling) */
	uart16550_write_reg(uart, UART_IER_DISABLE_ALL, UART16550_IER);

	/* Set baud rate */
	uart16550_set_baud_rate(BAUD_115200);

	/* Data terminal ready + Request to send */
	uart16550_write_reg(uart, MCR_RTS | MCR_DTR, UART16550_MCR);
}

char uart16550_getc(void)
{
	char ret = -1;

	if (!uart.enabled) {
		return ret;
	}

	spinlock_obtain(&uart.rx_lock);

	/* If a character has been received, read it */
	if ((uart16550_read_reg(uart, UART16550_LSR) & LSR_DR) == LSR_DR) {
		/* Read a character */
		ret = uart16550_read_reg(uart, UART16550_RBR);

	}
	spinlock_release(&uart.rx_lock);
	return ret;
}

/**
 * @pre uart->enabled == true
 */
static void uart16550_putc(char c)
{
	uint8_t temp;
	uint32_t reg;

	/* Ensure there are no further Transmit buffer write requests */
	do {
		reg = uart16550_read_reg(uart, UART16550_LSR);
	} while ((reg & LSR_THRE) == 0U || (reg & LSR_TEMT) == 0U);

	temp = (uint8_t)c;
	/* Transmit the character. */
	uart16550_write_reg(uart, (uint32_t)temp, UART16550_THR);
}

size_t uart16550_puts(const char *buf, uint32_t len)
{
	uint32_t i;
	if (!uart.enabled) {
		return len;
	}
	spinlock_obtain(&uart.tx_lock);
	for (i = 0U; i < len; i++) {
		/* Transmit character */
		uart16550_putc(*buf);
		if (*buf == '\n') {
			/* Append '\r', no need change the len */
			uart16550_putc('\r');
		}
		buf++;
	}
	spinlock_release(&uart.tx_lock);
	return len;
}

void uart16550_set_property(bool enabled, bool port_mapped, uint64_t base_addr)
{
	uart.enabled = enabled;
	uart.serial_port_mapped = port_mapped;

	if (port_mapped) {
		uart.port_address = base_addr;
	} else {
		const char *bdf = (const char *)base_addr;
		strncpy_s(pci_bdf_info, MAX_BDF_LEN + 1U, bdf, MAX_BDF_LEN);
	}
}

bool is_pci_dbg_uart(union pci_bdf bdf_value)
{
	bool ret = false;

	if (uart.enabled && !uart.serial_port_mapped) {
		if (bdf_value.value == serial_pci_bdf.value) {
			ret = true;
		}
	}

	return ret;
}
