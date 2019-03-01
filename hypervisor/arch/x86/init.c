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
static void enter_guest_mode(uint16_t pcpu_id)
{
	vmx_on();

	(void)launch_vms(pcpu_id);

	switch_to_idle(default_idle);

	/* Control should not come here */
	cpu_dead();
}

static void init_primary_cpu_post(void)
{
	/* Perform any necessary BSP initialization */
	init_bsp();

	init_debug_pre();

	init_cpu_post(BOOT_CPU_ID);

	init_debug_post(BOOT_CPU_ID);

	enter_guest_mode(BOOT_CPU_ID);
}

/* NOTE: this function is using temp stack, and after SWITCH_TO(runtime_sp, to)
 * it will switch to runtime stack.
 */
void init_primary_cpu(void)
{
	uint64_t rsp;

	init_cpu_pre(BOOT_CPU_ID);

	/* Switch to run-time stack */
	rsp = (uint64_t)(&get_cpu_var(stack)[CONFIG_STACK_SIZE - 1]);
	rsp &= ~(CPU_STACK_ALIGN - 1UL);
	SWITCH_TO(rsp, init_primary_cpu_post);
}

void init_secondary_cpu(void)
{
	uint16_t pcpu_id;

	init_cpu_pre(INVALID_CPU_ID);

	pcpu_id = get_cpu_id();

	init_cpu_post(pcpu_id);

	init_debug_post(pcpu_id);

	enter_guest_mode(pcpu_id);
}
