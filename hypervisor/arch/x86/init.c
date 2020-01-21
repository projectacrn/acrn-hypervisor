/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <init.h>
#include <console.h>
#include <per_cpu.h>
#include <profiling.h>
#include <vtd.h>
#include <shell.h>
#include <vmx.h>
#include <vm.h>
#include <logmsg.h>
#include <seed.h>
#include <uart16550.h>
#include <ld_sym.h>
#include <vboot.h>

/* Push sp magic to top of stack for call trace */
#define SWITCH_TO(rsp, to)                                              \
{                                                                       \
	asm volatile ("movq %0, %%rsp\n"                                \
			"pushq %1\n"                                    \
			"jmpq *%2\n"                                    \
			 :                                              \
			 : "r"(rsp), "rm"(SP_BOTTOM_MAGIC), "a"(to));   \
}

/*TODO: move into debug module */
static void init_debug_pre(void)
{
	/*
	 * Enable UART as early as possible.
	 * Then we could use printf for debugging on early boot stage.
	 */
	uart16550_init(true);

	/* Initialize console */
	console_init();

	/* Enable logging */
	init_logmsg(CONFIG_LOG_DESTINATION);
}

/*TODO: move into debug module */
static void init_debug_post(uint16_t pcpu_id)
{
	if (pcpu_id == BOOT_CPU_ID) {
		/* Initialize the shell */
		shell_init();
		console_setup_timer();
	}

	profiling_setup();
}

/*TODO: move into guest-vcpu module */
static void init_guest_mode(uint16_t pcpu_id)
{
	vmx_on();

	launch_vms(pcpu_id);
}

static void init_primary_pcpu_post(void)
{
	init_seed();

	init_pcpu_post(BOOT_CPU_ID);

	init_debug_post(BOOT_CPU_ID);

	init_guest_mode(BOOT_CPU_ID);

	run_idle_thread();
}

/* NOTE: this function is using temp stack, and after SWITCH_TO(runtime_sp, to)
 * it will switch to runtime stack.
 */
void init_primary_pcpu(void)
{
	uint64_t rsp;

	/* Clear BSS */
	(void)memset(&ld_bss_start, 0U, (size_t)(&ld_bss_end - &ld_bss_start));

	(void)parse_hv_cmdline();

	init_debug_pre();

	init_pcpu_pre(true);

	/* Switch to run-time stack */
	rsp = (uint64_t)(&get_cpu_var(stack)[CONFIG_STACK_SIZE - 1]);
	rsp &= ~(CPU_STACK_ALIGN - 1UL);
	SWITCH_TO(rsp, init_primary_pcpu_post);
}

void init_secondary_pcpu(void)
{
	uint16_t pcpu_id;

	init_pcpu_pre(false);

	pcpu_id = get_pcpu_id();

	init_pcpu_post(pcpu_id);

	init_debug_post(pcpu_id);

	init_guest_mode(pcpu_id);

	run_idle_thread();
}
