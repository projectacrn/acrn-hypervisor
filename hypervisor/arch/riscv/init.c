/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <types.h>
#include <cpu.h>
#include <per_cpu.h>
#include <logmsg.h>

static void init_pcpu_comm_post(void);

/* Push sp magic to top of stack for call trace */
#define SWITCH_TO(sp, to)							\
{										\
	asm volatile ( 								\
		"mv sp, %0\n"           /* Set stack pointer */			\
		"addi sp, sp, -8\n"     /* Make room for magic value */		\
		"sd %1, 0(sp)\n"        /* Store magic value */			\
		"jalr %2\n"             /* Jump to function pointer */		\
		:								\
		: "r"(sp), "r"(SP_BOTTOM_MAGIC), "r"(to)			\
	);									\
}

/* C entry point for boot CPU */
void init_primary_pcpu(uint64_t hart_id, uint64_t fdt_paddr)
{
	uint16_t pcpu_id = BSP_CPU_ID;
	uint64_t pcpu_sp;
	(void)fdt_paddr;

	init_percpu_hart_id(hart_id);
	set_pcpu_active(pcpu_id);

	/*
	 * Set state for this CPU to initializing, the current pcpu_id
	 * is set to a per-cpu register tp from now on.
	 */
	pcpu_set_current_state(pcpu_id, PCPU_STATE_INITIALIZING);

	if (!start_pcpus(AP_MASK)) {
		panic("Failed to start all secondary cores!");
	}

	/* Switch to run-time stack */
	pcpu_sp = (uint64_t)(&get_cpu_var(stack)[CONFIG_STACK_SIZE - 1]);
	pcpu_sp &= ~(CPU_STACK_ALIGN - 1UL);
	SWITCH_TO(pcpu_sp, init_pcpu_comm_post);
}

/* C entry point for AP */
void init_secondary_pcpu(uint64_t hart_id)
{
	uint16_t pcpu_id;

	pcpu_id = get_pcpu_id_from_hart_id(hart_id);
	if (pcpu_id >= MAX_PCPU_NUM) {
		panic("Invalid pCPU ID!");
	}

	set_pcpu_active(pcpu_id);

	/*
	 * Set state for this CPU to initializing, the current pcpu_id
	 * is set to a per-cpu register tp from now on.
	 */
	pcpu_set_current_state(pcpu_id, PCPU_STATE_INITIALIZING);

	/* This function shall never return */
	init_pcpu_comm_post();
}

static void init_pcpu_comm_post(void)
{
	uint16_t pcpu_id;

	pcpu_id = get_pcpu_id();

	/* to be implemented */
	(void)pcpu_id;

	while (1) {
		asm volatile("wfi" : : : "memory");
	}
}
