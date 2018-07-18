/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include "uart16550.h"
#include "serial_internal.h"

/* Mapping of 16c550 write-only registers to appropriate structure members */
#define THR_IDX	RBR_IDX
#define IIR_IDX	FCR_IDX
#define DLL_IDX	RBR_IDX
#define DLM_IDX	IER_IDX

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

enum UART_REG_IDX{
	RBR_IDX,		/* 0 */
	IER_IDX,		/* 1 */
	FCR_IDX,		/* 2 */
	LCR_IDX,		/* 3 */
	MCR_IDX,		/* 4 */
	ISR_IDX,		/* 5 */
	MSR_IDX,		/* 6 */
	SPR_IDX,		/* 7 */
	MDR1_IDX,		/* 8 */
	REG9_IDX,		/* 9 */
	REGA_IDX,		/* A */
	REGB_IDX,		/* B */
	REGC_IDX,		/* C */
	REGD_IDX,		/* D */
	REGE_IDX,		/* E */
	UASR_IDX,		/* F */
	SCR_IDX,		/* 10*/
	SSR_IDX,		/* 11*/
	REG12_IDX,		/* 12*/
	OSC_12M_SEL_IDX,	/* 13*/
};

/* CPU oscillator clock */
#define CPU_OSC_CLOCK	1843200	/* 1.8432 MHz */

/* UART hardware definitions */
#define UART_CLOCK_RATE		CPU_OSC_CLOCK
#define UART_BUFFER_SIZE		2048

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

static void uart16550_enable(__unused struct tgt_uart *tgt_uart)
{
}

static void uart16550_calc_baud_div(__unused struct tgt_uart *tgt_uart,
		uint32_t ref_freq, uint32_t *baud_div_ptr, uint32_t baud_rate)
{
	uint32_t baud_multiplier = baud_rate < BAUD_460800 ? 16 : 13;

	if (baud_rate == 0) {
		baud_rate = BAUD_115200;
	}
	*baud_div_ptr = ref_freq / (baud_multiplier * baud_rate);
}

static void uart16550_set_baud_rate(struct tgt_uart *tgt_uart,
			uint32_t baud_rate)
{
	uint32_t baud_div, duart_clock = CPU_OSC_CLOCK;
	uart_reg_t temp_reg;

	/* Calculate baud divisor */
	uart16550_calc_baud_div(
			tgt_uart, duart_clock, &baud_div, baud_rate);

	/* Enable DLL and DLM registers for setting the Divisor */
	temp_reg = uart16550_read_reg(tgt_uart->base_address, LCR_IDX);
	temp_reg |= LCR_DLAB;
	uart16550_write_reg(tgt_uart->base_address, temp_reg, LCR_IDX);

	/* Write the appropriate divisor value */
	uart16550_write_reg(tgt_uart->base_address,
			((baud_div >> 8) & 0xFFU), DLM_IDX);
	uart16550_write_reg(tgt_uart->base_address,
			(baud_div & 0xFFU), DLL_IDX);

	/* Disable DLL and DLM registers */
	temp_reg &= ~LCR_DLAB;
	uart16550_write_reg(tgt_uart->base_address, temp_reg, LCR_IDX);
}

static int uart16550_init(struct tgt_uart *tgt_uart)
{
	int status = 0;

	if (uart_enabled == 0) {
		/*uart will not be used */
		status = -ENODEV;
	} else {
		if (strcmp(tgt_uart->uart_id, "STDIO") == 0) {
			atomic_store32(&tgt_uart->open_count, 0U);
		} else {
			/* set open count to 1 to prevent open */
			atomic_store32(&tgt_uart->open_count, 1U);
			status = -EINVAL;
		}
	}

	return status;
}

static int uart16550_open(struct tgt_uart *tgt_uart,
			struct uart_config *config)
{
	uint32_t temp32;
	int status = 0;

	if (strcmp(tgt_uart->uart_id, "STDIO") == 0) {
		if (atomic_cmpxchg32(&tgt_uart->open_count, 0U, 1U) != 0U) {
			return -EBUSY;
		}

		/* Call UART setup function */
		/* Enable TX and RX FIFOs */
		uart16550_write_reg(tgt_uart->base_address,
				FCR_FIFOE | FCR_RFR | FCR_TFR, FCR_IDX);

		/* Set parity value */
		if (config->parity_bits == PARITY_ODD) {
			/* Odd parity */
			temp32 = LCR_PARITY_ODD;
		} else if (config->parity_bits == PARITY_EVEN) {
			/* Even parity */
			temp32 = LCR_PARITY_EVEN;
		} else {
			/* No parity */
			temp32 = LCR_PARITY_NONE;
		}

		/* Set Data length */
		if (config->data_bits == DATA_7) {
			/* Set bits for 7 data bits */
			temp32 |= LCR_WL7;
		} else {
			/* Set bits for 8 data bits */
			temp32 |= LCR_WL8;
		}

		/* Check for 1 stop bit */
		if (config->stop_bits == STOP_1) {
			/* Set bits for 1 stop bit */
			temp32 |= LCR_NB_STOP_BITS_1;
		} else {
			/* Set bits for 2 stop bits */
			temp32 |= LCR_NB_STOP_BITS_2;
		}

		/* Set-up data bits / parity / stop bits. */
		uart16550_write_reg(tgt_uart->base_address,
					temp32, LCR_IDX);

		/* Disable interrupts (we use polling) */
		uart16550_write_reg(tgt_uart->base_address,
				UART_IER_DISABLE_ALL, IER_IDX);

		/* Set baud rate */
		uart16550_set_baud_rate(tgt_uart, config->baud_rate);

		/* Data terminal ready + Request to send */
		uart16550_write_reg(tgt_uart->base_address,
				MCR_RTS | MCR_DTR, MCR_IDX);

		/* Enable the UART hardware */
		uart16550_enable(tgt_uart);
	} else {
		status = -ENODEV;
	}

	return status;
}

static uint32_t uart16550_get_rx_err(uint32_t rx_data)
{
	uint32_t rx_status = SD_RX_NO_ERROR;

	/* Check for RX overrun error */
	if ((rx_data & LSR_OE) != 0U) {
		rx_status |= SD_RX_OVERRUN_ERROR;
	}

	/* Check for RX parity error */
	if ((rx_data & LSR_PE) != 0U) {
		rx_status |= SD_RX_PARITY_ERROR;
	}

	/* Check for RX frame error */
	if ((rx_data & LSR_FE) != 0U) {
		rx_status |= SD_RX_FRAME_ERROR;
	}

	/* Return the rx status */
	return rx_status;
}

static void uart16550_close(struct tgt_uart *tgt_uart)
{
	if (tgt_uart != NULL) {
		if (atomic_cmpxchg32(&tgt_uart->open_count, 1U, 0U) == 1U) {
			/* TODO: Add logic to disable the UART */
		}
	}
}

static void uart16550_read(struct tgt_uart *tgt_uart, void *buffer,
		uint32_t *bytes_read)
{
	/* If a character has been received, read it */
	if ((uart16550_read_reg(tgt_uart->base_address, ISR_IDX) & LSR_DR)
			== LSR_DR) {
		/* Read a character */
		*(uint8_t *)buffer =
			uart16550_read_reg(tgt_uart->base_address, RBR_IDX);

		/* Read 1 byte */
		*bytes_read = 1U;
	} else {
		*bytes_read = 0U;
	}
}

static void uart16550_write(struct tgt_uart *tgt_uart,
		const void *buffer, uint32_t *bytes_written)
{
	/* Ensure there are no further Transmit buffer write requests */
	do {
	} while ((uart16550_read_reg(tgt_uart->base_address,
		ISR_IDX) & LSR_THRE) == 0U);

	/* Transmit the character. */
	uart16550_write_reg(tgt_uart->base_address,
		*(uint8_t *)buffer, THR_IDX);

	if (bytes_written != NULL) {
		*bytes_written = 1;
	}
}

static bool uart16550_tx_is_busy(struct tgt_uart *tgt_uart)
{
	return ((uart16550_read_reg(tgt_uart->base_address, ISR_IDX) &
				(LSR_TEMT)) == 0) ? true : false;
}

static bool uart16550_rx_data_is_avail(struct tgt_uart *tgt_uart,
			uint32_t *lsr_reg)
{
	*(uart_reg_t *)lsr_reg =
		 uart16550_read_reg(tgt_uart->base_address, ISR_IDX);
	return ((*(uart_reg_t *)lsr_reg & LSR_DR) == LSR_DR) ? true : false;
}

struct tgt_uart Tgt_Uarts[SERIAL_MAX_DEVS] = {
	{
		.uart_id		= "STDIO",
		.base_address		= UART_BASE_ADDRESS,
		.clock_frequency	= UART_CLOCK_RATE,
		.buffer_size		= UART_BUFFER_SIZE,
		.init			= uart16550_init,
		.open			= uart16550_open,
		.close			= uart16550_close,
		.read			= uart16550_read,
		.write			= uart16550_write,
		.tx_is_busy		= uart16550_tx_is_busy,
		.rx_data_is_avail	= uart16550_rx_data_is_avail,
		.get_rx_err		= uart16550_get_rx_err,

	}
};

void uart16550_set_property(int enabled, int port_mapped, uint64_t base_addr)
{
	uart_enabled = enabled;
	serial_port_mapped = port_mapped;
	Tgt_Uarts[0].base_address = base_addr;
}
