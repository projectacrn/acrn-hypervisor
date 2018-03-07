/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
#include "serial_internal.h"

static spinlock_t lock;

static uint32_t serial_handle = SERIAL_INVALID_HANDLE;

#define CONSOLE_KICK_TIMER_TIMEOUT  40 /* timeout is 40ms*/

uint32_t get_serial_handle(void)
{
	return serial_handle;
}

static int print_char(char x)
{
	serial_puts(serial_handle, &x, 1);

	if (x == '\n')
		serial_puts(serial_handle, "\r", 1);

	return 0;
}

int console_init(void)
{
	spinlock_init(&lock);

	serial_handle = serial_open("STDIO");

	return 0;
}

int console_putc(int ch)
{
	int res = -1;

	spinlock_obtain(&lock);

	if (serial_handle != SERIAL_INVALID_HANDLE)
		res = print_char(ch);

	spinlock_release(&lock);

	return res;
}

int console_puts(const char *s)
{
	int res = -1;
	const char *p;

	spinlock_obtain(&lock);

	if (serial_handle != SERIAL_INVALID_HANDLE) {
		res = 0;
		while (*s) {
			/* start output at the beginning of the string search
			 * for end of string or '\n'
			 */
			p = s;

			while (*p && *p != '\n')
				++p;

			/* write all characters up to p */
			serial_puts(serial_handle, s, p - s);

			res += p - s;

			if (*p == '\n') {
				print_char('\n');
				++p;
				res += 2;
			}

			/* continue at position p */
			s = p;
		}
	}

	spinlock_release(&lock);

	return res;
}

int console_write(const char *s, size_t len)
{
	int res = -1;
	const char *e;
	const char *p;

	spinlock_obtain(&lock);

	if (serial_handle != SERIAL_INVALID_HANDLE) {
		/* calculate pointer to the end of the string */
		e = s + len;
		res = 0;

		/* process all characters */
		while (s != e) {
			/* search for '\n' or the end of the string */
			p = s;

			while ((p != e) && (*p != '\n'))
				++p;

			/* write all characters processed so far */
			serial_puts(serial_handle, s, p - s);

			res += p - s;

			/* write '\n' if end of string is not reached */
			if (p != e) {
				print_char('\n');
				++p;
				res += 2;
			}

			/* continue at next position */
			s = p;
		}
	}

	spinlock_release(&lock);

	return res;
}

void console_dump_bytes(const void *p, unsigned int len)
{

	const unsigned char *x = p;
	const unsigned char *e = x + len;
	int i;

	/* dump all bytes */
	while (x < e) {
		/* write the address of the first byte in the row */
		printf("%08x: ", (vaddr_t) x);
		/* print one row (16 bytes) as hexadecimal values */
		for (i = 0; i < 16; i++)
			printf("%02x ", x[i]);

		/* print one row as ASCII characters (if possible) */
		for (i = 0; i < 16; i++) {
			if ((x[i] < ' ') || (x[i] >= 127))
				console_putc('.');
			else
				console_putc(x[i]);
		}
		/* continue with next row */
		console_putc('\n');
		/* set pointer one row ahead */
		x += 16;
	}
}

static void console_read(void)
{
	spinlock_obtain(&lock);

	if (serial_handle != SERIAL_INVALID_HANDLE) {
		/* Get all the data available in the RX FIFO */
		serial_get_rx_data(serial_handle);
	}

	spinlock_release(&lock);
}

static void console_handler(void)
{
	/* Dump the RX FIFO to a circular buffer */
	console_read();

	/* serial Console Rx operation */
	vuart_console_rx_chars(serial_handle);

	/* serial Console Tx operation */
	vuart_console_tx_chars();

	shell_kick_session();
}

static int console_timer_callback(__unused uint64_t data)
{
	/* Kick HV-Shell and Uart-Console tasks */
	console_handler();

	/* Restart the timer */
	console_setup_timer();

	return 0;
}

void console_setup_timer(void)
{
	/* Start an one-shot timer */
	if (add_timer(console_timer_callback, 0,
		rdtsc() + TIME_MS_DELTA * CONSOLE_KICK_TIMER_TIMEOUT) < 0)
		pr_err("Failed to add console kick timer");
}
