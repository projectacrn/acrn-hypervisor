/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <pci.h>
#include <uart16550.h>
#include <shell.h>
#include <timer.h>
#include <vuart.h>
#include <logmsg.h>

struct hv_timer console_timer;

#define CONSOLE_KICK_TIMER_TIMEOUT  40UL /* timeout is 40ms*/

void console_init(void)
{
	uart16550_init();
}

void console_putc(const char *ch)
{
	(void)uart16550_puts(ch, 1U);
}


size_t console_write(const char *s, size_t len)
{
	return  uart16550_puts(s, len);
}

char console_getc(void)
{
	return uart16550_getc();
}

static void console_timer_callback(__unused void *data)
{
	struct acrn_vuart *vu;

	/* Kick HV-Shell and Uart-Console tasks */
	vu = vuart_console_active();
	if (vu != NULL) {
		/* serial Console Rx operation */
		vuart_console_rx_chars(vu);
		/* serial Console Tx operation */
		vuart_console_tx_chars(vu);
	} else {
		shell_kick();
	}
}

void console_setup_timer(void)
{
	uint64_t period_in_cycle, fire_tsc;

	period_in_cycle = CYCLES_PER_MS * CONSOLE_KICK_TIMER_TIMEOUT;
	fire_tsc = rdtsc() + period_in_cycle;
	initialize_timer(&console_timer,
			console_timer_callback, NULL,
			fire_tsc, TICK_MODE_PERIODIC, period_in_cycle);

	/* Start an periodic timer */
	if (add_timer(&console_timer) != 0) {
		pr_err("Failed to add console kick timer");
	}
}

void suspend_console(void)
{
	del_timer(&console_timer);
}

void resume_console(void)
{
	console_setup_timer();
}
