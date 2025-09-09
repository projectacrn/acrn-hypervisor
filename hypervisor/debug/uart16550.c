/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <spinlock.h>
#include <pci.h>
#include <uart16550.h>
#include <asm/io.h>
#include <asm/cpu.h>
#include <asm/mmu.h>

#define MAX_BDF_LEN 8

struct console_uart {
	bool enabled;

	enum serial_dev_type type;
	uint16_t port_address;
	void *mmio_base_vaddr;
	union pci_bdf bdf;

	spinlock_t rx_lock;
	spinlock_t tx_lock;

	uint32_t reg_width;
};

#if defined(CONFIG_SERIAL_PIO_BASE)
static struct console_uart uart = {
	.enabled = true,
	.type = PIO,
	.port_address = CONFIG_SERIAL_PIO_BASE,
	.reg_width = 1,
};
#elif defined(CONFIG_SERIAL_PCI_BDF)
static struct console_uart uart = {
	.enabled = true,
	.type = PCI,
	.bdf.value = CONFIG_SERIAL_PCI_BDF,
	.reg_width = 4,
};
#elif defined(CONFIG_SERIAL_MMIO_BASE)
static struct console_uart uart = {
	.enabled = true,
	.type = MMIO,
	.mmio_base_vaddr = (void *)CONFIG_SERIAL_MMIO_BASE,
	.reg_width = 1,
};
#endif

typedef uint32_t uart_reg_t;

/**
 * @pre uart->enabled == true
 */
static inline uint32_t uart16550_read_reg(struct console_uart uart, uint16_t reg_idx)
{
	if (uart.type == PIO) {
		return pio_read8(uart.port_address + (reg_idx * uart.reg_width));
	} else {
		if (uart.reg_width == 4U) {
			return mmio_read32(uart.mmio_base_vaddr + (reg_idx * uart.reg_width));
		} else {
			return mmio_read8(uart.mmio_base_vaddr + (reg_idx * uart.reg_width));
		}
	}
}

/**
 * @pre uart->enabled == true
 */
static inline void uart16550_write_reg(struct console_uart uart, uint32_t val, uint16_t reg_idx)
{
	if (uart.type == PIO) {
		pio_write8(val, uart.port_address + (reg_idx * uart.reg_width));
	} else {
		if (uart.reg_width == 4U) {
			mmio_write32(val, uart.mmio_base_vaddr + (reg_idx * uart.reg_width));
		} else {
			mmio_write8(val, uart.mmio_base_vaddr + (reg_idx * uart.reg_width));
		}
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

static uint8_t uart_pde_page[PAGE_SIZE]__aligned(PAGE_SIZE);
static uint8_t uart_pdpte_page[PAGE_SIZE]__aligned(PAGE_SIZE);

static void early_pgtable_map_uart(uint64_t addr)
{
	uint64_t *pml4e, *pdpte, *pde;
	uint64_t value;

	CPU_CR_READ(cr3, &value);
	/*assumpiton for map high mmio in early pagetable is that it is only used for
	  2MB page since 1G page may not available when memory width is 39bit */
	pml4e = pml4e_offset((uint64_t *)value, addr);
	/* address is above 512G */
	if(!(*pml4e & PAGE_PRESENT)) {
		*pml4e = hva2hpa_early(uart_pdpte_page) + (PAGE_PRESENT|PAGE_RW);
	}
	pdpte = pdpte_offset(pml4e, addr);
	if(!(*pdpte & PAGE_PRESENT)) {
		*(pdpte) = hva2hpa_early(uart_pde_page) + (PAGE_PRESENT|PAGE_RW);
		pde = pde_offset(pdpte, addr);
		*pde =  (addr & PDE_MASK) + (PAGE_PRESENT|PAGE_RW|PAGE_PSE);
	} else if(!(*pdpte & PAGE_PSE)) {
		pde = pde_offset(pdpte, addr);
		if(!(*pde & PAGE_PRESENT)) {
			*pde = (addr & PDE_MASK) + (PAGE_PRESENT|PAGE_RW|PAGE_PSE);
		}
	}
}

void uart16550_init(bool early_boot)
{
	void *mmio_base_va = NULL;

	if (!uart.enabled) {
		return;
	}

	if (!early_boot) {
		if (uart.type == MMIO) {
			mmio_base_va = hpa2hva(hva2hpa_early(uart.mmio_base_vaddr));
			if (mmio_base_va != NULL) {
				set_paging_supervisor((uint64_t)mmio_base_va, PDE_SIZE);
			}
		}
		return;
	}

	/* if configure serial PCI BDF, get its base MMIO address */
	if (uart.type == PCI) {
		uint32_t bar0 = pci_pdev_read_cfg(uart.bdf, pci_bar_offset(0), 4U);

		if ((bar0 & ~0xfU) == 0U) {
			/* in case the PCI UART BAR is reset to 0 after boot */
			uart.enabled = false;
			return;
		} else {
			uint16_t cmd = (uint16_t)pci_pdev_read_cfg(uart.bdf, PCIR_COMMAND, 2U);
			if ((bar0 & 0x3U) == PCIM_BAR_IO_SPACE) { /* IO Space */
				uart.type = PIO;
				uart.port_address = (uint16_t)(bar0 & PCI_BASE_ADDRESS_IO_MASK);
				uart.reg_width = 1;
				pci_pdev_write_cfg(uart.bdf, PCIR_COMMAND, 2U, cmd | PCIM_CMD_PORTEN);
			} else if (((bar0 & 0x7U) == 0U) || ((bar0 & 0x7U) == 4U)) {
					uart.type = MMIO;
					uint32_t bar_hi = pci_pdev_read_cfg(uart.bdf, pci_bar_offset(1), 4U);
					uint64_t addr = (bar0 & PCI_BASE_ADDRESS_MEM_MASK)|(((uint64_t)bar_hi) << 32U);
					if (bar_hi != 0U) {
						early_pgtable_map_uart(addr);
					}
					uart.mmio_base_vaddr = hpa2hva_early(addr);
					pci_pdev_write_cfg(uart.bdf, PCIR_COMMAND, 2U, cmd | PCIM_CMD_MEMEN);
			} else {
				uart.enabled = false;
				return;
			}
		}
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
	uint64_t rflags;

	if (!uart.enabled) {
		return ret;
	}

	spinlock_irqsave_obtain(&uart.rx_lock, &rflags);
	/* If a character has been received, read it */
	if ((uart16550_read_reg(uart, UART16550_LSR) & LSR_DR) == LSR_DR) {
		/* Read a character */
		ret = uart16550_read_reg(uart, UART16550_RBR);

	}
	spinlock_irqrestore_release(&uart.rx_lock, rflags);
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
	uint64_t rflags;

	if (!uart.enabled) {
		return len;
	}

	spinlock_irqsave_obtain(&uart.tx_lock, &rflags);
	for (i = 0U; i < len; i++) {
		/* Transmit character */
		uart16550_putc(*buf);
		if (*buf == '\n') {
			/* Append '\r', no need change the len */
			uart16550_putc('\r');
		}
		buf++;
	}
	spinlock_irqrestore_release(&uart.tx_lock, rflags);
	return len;
}

void uart16550_set_property(bool enabled, enum serial_dev_type uart_type, uint64_t data)
{
	uart.enabled = enabled;
	uart.type = uart_type;
	uart.bdf.value = 0U;

	if (uart_type == PIO) {
		uart.port_address = data;
	} else if (uart_type == PCI) {
		uart.bdf.value = data;
		uart.reg_width = 4;
	} else if (uart_type == MMIO) {
		uart.mmio_base_vaddr = (void *)data;
		uart.reg_width = 1;
	}
}

bool is_pci_dbg_uart(union pci_bdf bdf_value)
{
	bool ret = false;

	if (uart.enabled && (uart.bdf.value != 0)) {
		if (bdf_value.value == uart.bdf.value) {
			ret = true;
		}
	}

	return ret;
}

bool get_pio_dbg_uart_cfg(uint16_t *pio_address, uint32_t *nbytes)
{
	bool ret = false;

	if (uart.enabled && (uart.type == PIO)) {
		*pio_address = uart.port_address;
		*nbytes = 8U;
		ret = true;
	}

	return ret;
}
