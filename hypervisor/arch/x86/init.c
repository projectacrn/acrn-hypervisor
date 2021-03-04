/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <x86/init.h>
#include <console.h>
#include <x86/per_cpu.h>
#include <shell.h>
#include <x86/vmx.h>
#include <x86/guest/vm.h>
#include <logmsg.h>
#include <x86/seed.h>
#include <x86/boot/ld_sym.h>
#include <multiboot.h>

/* boot_regs store the multiboot info magic and address, defined in
   arch/x86/boot/cpu_primary.S.
   */
extern uint32_t boot_regs[2];

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
	if (pcpu_id == BSP_CPU_ID) {
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

static void init_pcpu_comm_post(void)
{
	uint16_t pcpu_id;

	pcpu_id = get_pcpu_id();

	init_pcpu_post(pcpu_id);
	init_debug_post(pcpu_id);
	init_guest_mode(pcpu_id);
	run_idle_thread();
}

static void init_misc(void)
{
	init_cr0_cr4_flexible_bits();
	if (!sanitize_cr0_cr4_pattern()) {
		panic("%s Sanitize pattern of CR0 or CR4 failed.\n", __func__);
	}
}

/* NOTE: this function is using temp stack, and after SWITCH_TO(runtime_sp, to)
 * it will switch to runtime stack.
 */
void init_primary_pcpu(void)
{
	uint64_t rsp;

	/* Clear BSS */
	(void)memset(&ld_bss_start, 0U, (size_t)(&ld_bss_end - &ld_bss_start));

	init_acrn_multiboot_info(boot_regs[0], boot_regs[1]);

	init_debug_pre();

	if (sanitize_acrn_multiboot_info(boot_regs[0], boot_regs[1]) != 0) {
		panic("Multiboot info error!");
	}

	init_pcpu_pre(true);

	init_seed();
	init_misc();

	/* Switch to run-time stack */
	rsp = (uint64_t)(&get_cpu_var(stack)[CONFIG_STACK_SIZE - 1]);
	rsp &= ~(CPU_STACK_ALIGN - 1UL);
	SWITCH_TO(rsp, init_pcpu_comm_post);
}

void init_secondary_pcpu(void)
{
	init_pcpu_pre(false);
	init_pcpu_comm_post();
}
