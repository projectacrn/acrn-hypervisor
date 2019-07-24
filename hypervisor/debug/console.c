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
#include <acrn_hv_defs.h>
#include <vm.h>
#include <console.h>

struct hv_timer console_timer;

#define CONSOLE_KICK_TIMER_TIMEOUT  40UL /* timeout is 40ms*/
/* Switching key combinations for shell and uart console */
#define GUEST_CONSOLE_TO_HV_SWITCH_KEY      0       /* CTRL + SPACE */
uint16_t console_vmid = ACRN_INVALID_VMID;

void console_init(void)
{
	uart16550_init(false);
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

/*
 * @post return != NULL
 */
struct acrn_vuart *vm_console_vuart(struct acrn_vm *vm)
{
	return &vm->vuart[0];
}

/**
 * @pre vu != NULL
 * @pre vu->active == true
 */
static void vuart_console_rx_chars(struct acrn_vuart *vu)
{
	char ch = -1;

	/* Get data from physical uart */
	ch = uart16550_getc();

	if (ch == GUEST_CONSOLE_TO_HV_SWITCH_KEY) {
		/* Switch the console */
		console_vmid = ACRN_INVALID_VMID;
		printf("\r\n\r\n ---Entering ACRN SHELL---\r\n");
	}
	if (ch != -1) {
		vuart_putchar(vu, ch);
		vuart_toggle_intr(vu);
	}

}

/**
 * @pre vu != NULL
 */
static void vuart_console_tx_chars(struct acrn_vuart *vu)
{
	char c = vuart_getchar(vu);

	while(c != -1) {
		printf("%c", c);
		c = vuart_getchar(vu);
	}
}

static struct acrn_vuart *vuart_console_active(void)
{
	struct acrn_vm *vm = NULL;
	struct acrn_vuart *vu = NULL;

	if (console_vmid < CONFIG_MAX_VM_NUM) {
		vm = get_vm_from_vmid(console_vmid);
		if (!is_poweroff_vm(vm)) {
			vu = vm_console_vuart(vm);
		} else {
			/* Console vm is invalid, switch back to HV-Shell */
			console_vmid = ACRN_INVALID_VMID;
		}
	}

	return ((vu != NULL) && vu->active) ? vu : NULL;
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
