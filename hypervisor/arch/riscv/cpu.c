/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 *
 */


#include <types.h>
#include <per_cpu.h>
#include <cpu.h>
#include <delay.h>
#include <asm/sbi.h>
#include <asm/pgtable.h>

extern void _start_secondary_sbi(uint64_t phy_stack_addr);

/* wait until *sync == wake_sync */
void wait_sync_change(volatile const uint64_t *sync, uint64_t wake_sync)
{
	while ((*sync) != wake_sync) {
		cpu_relax();
	}
}

uint16_t arch_get_pcpu_num(void)
{
	return NR_CPUS;
}

void arch_start_pcpu(uint16_t pcpu_id)
{
	int64_t ret;
	uint64_t pcpu_sp, phy_stack_addr;
	uint32_t hart_id = per_cpu(arch.hart_id, pcpu_id);
	uint64_t phy_start_addr = hva2hpa((void *)_start_secondary_sbi);

	pcpu_sp = (uint64_t)&per_cpu(stack, pcpu_id)[CONFIG_STACK_SIZE - 1];
	pcpu_sp &= ~(CPU_STACK_ALIGN - 1UL);
	phy_stack_addr = hva2hpa((void *)pcpu_sp);

	ret = sbi_hsm_start_hart(hart_id, phy_start_addr, phy_stack_addr);
	if (ret != SBI_SUCCESS) {
		pr_fatal("Failed to start cpu%hu by SBI HSM", pcpu_id);
	}
}
