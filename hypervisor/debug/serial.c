/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include "serial_internal.h"

static struct uart *sio_ports[SERIAL_MAX_DEVS];
static uint8_t sio_initialized[SERIAL_MAX_DEVS];

static struct uart *get_uart_by_id(char *uart_id, uint32_t *index)
{
	/* Initialize the index to the start of array. */
	*index = 0;

	while (sio_ports[*index] != NULL) {
		if (strncmp(sio_ports[*index]->tgt_uart->uart_id, uart_id,
			strnlen_s(sio_ports[*index]->tgt_uart->uart_id,
				SERIAL_ID_MAX_LENGTH)) == 0)
			break;

		/* No device is found if index reaches end of array. */
		if (++(*index) == SERIAL_MAX_DEVS)
			return NULL;

	}
	return sio_ports[*index];
}

int serial_init(void)
{
	uint32_t index = 0;
	int status = 0;

	while (index < SERIAL_MAX_DEVS) {
		/* Allocate memory for generic control block of enabled UART */
		sio_ports[index] = calloc(1, sizeof(struct uart));

		if (!sio_ports[index]) {
			status = -ENOMEM;
			break;
		}

		sio_ports[index]->tgt_uart = &(Tgt_Uarts[index]);

		/*
		 * Set the open flag to false to indicate that UART port is
		 * not opened yet.
		 */
		sio_ports[index]->open_flag = false;

		/* Reset the tx lock */
		spinlock_init(&sio_ports[index]->tx_lock);

		sio_ports[index]->rx_sio_queue = sbuf_allocate(
				sio_ports[index]->tgt_uart->buffer_size,
				sizeof(uint8_t));
		if (sio_ports[index]->rx_sio_queue != NULL) {
			sbuf_set_flags(sio_ports[index]->rx_sio_queue,
					OVERWRITE_EN);

			/* Call target specific initialization function */
			status = sio_ports[index]->tgt_uart->
				init(sio_ports[index]->tgt_uart);

			if (status == 0)
				sio_initialized[index] = true;
		} else {
			status = -ENOMEM;
			break;
		}

		index++;
	}

	return status;
}

uint32_t serial_open(char *uart_id)
{
	int status = SERIAL_DEV_NOT_FOUND;
	struct uart *uart;
	uint32_t index;

	/* Get UART control block from given character ID */
	uart = get_uart_by_id(uart_id, &index);

	if (uart != NULL && index < SERIAL_MAX_DEVS &&
			sio_initialized[index] &&
			(uart->open_flag == false)) {
		/* Reset the buffer lock */
		spinlock_init(&uart->buffer_lock);

		/* Configure the UART port to default settings. */
		uart->config.data_bits = DATA_8;
		uart->config.stop_bits = STOP_1;
		uart->config.parity_bits = PARITY_NONE;
		uart->config.baud_rate = BAUD_115200;
		uart->config.flow_control = FLOW_NONE;
		uart->config.read_mode = SUSPEND;

		/* Open the UART hardware with default configuration. */
		status = uart->tgt_uart->open(uart->tgt_uart, &(uart->config));

		if (status == 0)
			uart->open_flag = true;
	}

	/* Already open serial device */
	else if (uart != NULL && uart->open_flag == true) {
		/* Reset the buffer lock */
		spinlock_init(&uart->buffer_lock);
		status = 0;
	}

	return (status == 0) ?
			SERIAL_ENCODE_INDEX(index) :
			SERIAL_INVALID_HANDLE;
}

int serial_get_rx_data(uint32_t uart_handle)
{
	uint32_t index;
	struct uart *uart;
	int data_avail, rx_byte_status;
	uint32_t lsr_reg, bytes_read;
	uint8_t ch;
	int total_bytes_read = 0;

	if (!SERIAL_VALIDATE_HANDLE(uart_handle))
		return 0;

	index = SERIAL_DECODE_INDEX(uart_handle);
	if (index >= SERIAL_MAX_DEVS)
		return 0;

	uart = sio_ports[index];
	if (uart == NULL)
		return 0;

	/* Place all the data available in RX FIFO, in circular buffer */
	while ((data_avail = uart->tgt_uart->rx_data_is_avail(
				uart->tgt_uart, &lsr_reg))) {

		/* Read the byte */
		uart->tgt_uart->read(uart->tgt_uart, (void *)&ch, &bytes_read);

		/* Get RX status for this byte */
		rx_byte_status = uart->tgt_uart->get_rx_err(lsr_reg);

		/*
		 * Check if discard errors in RX character
		 * (parity / framing errors)
		 */
		if (rx_byte_status >= SD_RX_PARITY_ERROR) {
			/* Increase error status if bad data */
			uart->rx_error.parity_errors +=
				(rx_byte_status == SD_RX_PARITY_ERROR);
			uart->rx_error.frame_errors +=
				(rx_byte_status == SD_RX_FRAME_ERROR);
		} else {
			/* Update the overrun errors */
			uart->rx_error.overrun_errors +=
				(rx_byte_status == SD_RX_OVERRUN_ERROR);

			/* Enter Critical Section */
			spinlock_obtain(&uart->buffer_lock);

			/* Put the item on circular buffer */
			sbuf_put(uart->rx_sio_queue, &ch);

			/* Exit Critical Section */
			spinlock_release(&uart->buffer_lock);
		}
		/* Update the total bytes read */
		total_bytes_read += bytes_read;
	}
	return total_bytes_read;
}

int serial_getc(uint32_t uart_handle)
{
	uint8_t ch;
	struct uart *port;
	uint32_t index;
	int status = SERIAL_DEV_NOT_FOUND;

	if (!SERIAL_VALIDATE_HANDLE(uart_handle))
		goto exit;

	index = SERIAL_DECODE_INDEX(uart_handle);

	if (index >= SERIAL_MAX_DEVS)
		goto exit;

	port = sio_ports[index];

	if (port == NULL)
		goto exit;

	/* First read a character from the circular buffer regardless of the
	 * read mode of UART port. If status is not CBUFFER_EMPTY, character
	 * read from UART port is returned to the caller. Otherwise, if read
	 * mode is not NO_SUSPEND, thread is blocked until a character is read
	 * from the port. Serial target specific HISR unblocks the thread when
	 * a character is received and character is then read from the circular
	 * buffer.
	 */

	/* Disable interrupts for critical section */
	spinlock_obtain(&port->buffer_lock);

	status = sbuf_get(port->rx_sio_queue, &ch);

	/* Restore interrupts to original level. */
	spinlock_release(&port->buffer_lock);

exit:
	/* Return the character read, otherwise return the error status */
	return ((status > 0) ? (int)(ch) : SERIAL_EOF);
}

int serial_gets(uint32_t uart_handle, char *buffer, uint32_t length)
{
	char *data_read = buffer;
	int c;
	struct uart *port;
	uint32_t index;
	int status = 0;

	if ((buffer == NULL) || (length == 0))
		return 0;

	if (!SERIAL_VALIDATE_HANDLE(uart_handle))
		return 0;

	index = SERIAL_DECODE_INDEX(uart_handle);
	if (index >= SERIAL_MAX_DEVS)
		return 0;

	port = sio_ports[index];
	if ((port != NULL) && (port->open_flag == true)) {
		for (; length > 0; data_read++, length--) {
			/* Disable interrupts for critical section */
			spinlock_obtain(&port->buffer_lock);

			status = sbuf_get(port->rx_sio_queue, (uint8_t *)&c);

			/* Restore interrupts to original level. */
			spinlock_release(&port->buffer_lock);

			if (status <= 0)
				break;

			/* Save character in buffer */
			*data_read = (char) c;
		}
	}
	/* Return actual number of bytes read */
	return (int)(data_read - buffer);
}

static int serial_putc(uint32_t uart_handle, int c)
{
	uint32_t index, bytes_written = 0;
	struct uart *uart;
	int busy;

	if (!SERIAL_VALIDATE_HANDLE(uart_handle))
		return SERIAL_EOF;

	index = SERIAL_DECODE_INDEX(uart_handle);

	if (index >= SERIAL_MAX_DEVS)
		return SERIAL_EOF;

	uart = sio_ports[index];

	if (uart == NULL)
		return SERIAL_EOF;

	/* Wait for TX hardware to be ready */
	do {
		busy = uart->tgt_uart->tx_is_busy(uart->tgt_uart);
	} while (busy);

	/* Transmit character */
	uart->tgt_uart->write(uart->tgt_uart, &(c), &bytes_written);

	/* Return character written or EOF for error */
	return ((bytes_written > 0) ? c : (SERIAL_EOF));
}

int serial_puts(uint32_t uart_handle, const char *s, uint32_t length)
{
	const char *old_data = s;
	uint32_t index;
	struct uart *port;
	int retval = 0;

	if ((s == NULL) || (length == 0))
		return 0;

	if (!SERIAL_VALIDATE_HANDLE(uart_handle))
		return 0;

	index = SERIAL_DECODE_INDEX(uart_handle);

	if (index >= SERIAL_MAX_DEVS)
		return 0;

	port = sio_ports[index];

	if (port == NULL)
		return 0;

	/*
	 * Grab the semaphore so that strings between threads do not
	 * get mixed.
	 */
	spinlock_obtain(&port->tx_lock);

	/*
	 * Loop through the string until desired length of bytes have
	 * been written or SERIAL_EOF is returned.
	 */
	for (; length > 0 && retval != SERIAL_EOF; s++, length--)
		retval = serial_putc(uart_handle, (int) *s);

	/* Allow other threads to use this service. */
	spinlock_release(&port->tx_lock);

	/* Return actual number of bytes written */
	return (int)(s - old_data);
}
