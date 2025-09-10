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

/*
 * This array contains the hart IDs for each physial cpu.
 * FIXME: It should be populated by config tool by parsing
 * the device tree and then it can be declared as const.
 */
static uint32_t dt_hart_ids[MAX_PCPU_NUM];

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

/**
 * @brief Initialize the mapping between logical CPU ID and physical HART iD.
 *
 * This function sets up the mapping between logical CPU IDs and physical hart
 * IDs. The logical BSP_CPU_ID is always mapped to the actual BSP hart ID
 * provided as the input argument bsp_hart_id.
 *
 * @param bsp_hart_id The hardware hart ID to be assigned to the BSP logical CPU.
 */
void init_percpu_hart_id(uint32_t bsp_hart_id)
{
	uint16_t i;
	uint16_t idx = 0;

	/* FIXME: Remove below initialization when dt_hart_ids
	 * can be populated by config tool
	 */
	for (i = 0; i < MAX_PCPU_NUM; i++) {
		dt_hart_ids[i] = i;
	}

	/* ACRN is using BSP_CPU_ID as the BSP logical CPU ID */
	i = 0U;
	while (i < MAX_PCPU_NUM) {
		if (i == BSP_CPU_ID) {
			per_cpu(arch.hart_id, i) = bsp_hart_id;
			i++;
		} else if (idx < MAX_PCPU_NUM) {
			if (dt_hart_ids[idx] != bsp_hart_id) {
				per_cpu(arch.hart_id, i) = dt_hart_ids[idx];
				i++;
			}
			idx++;
		} else {
			/*
			 * Duplicate hart ID detected: multiple harts share the
			 * same hart ID as the BSP. This violates the RISC-V
			 * specification and indicates a critical system
			 * configuration error.
			 */
			 panic("BSP hart ID is not unique!");
		}
	}
}

uint16_t get_pcpu_id_from_hart_id(uint32_t hart_id)
{
	uint16_t i;
	uint16_t pcpu_id = INVALID_CPU_ID;

	for (i = 0U; i < MAX_PCPU_NUM; i++) {
		if (per_cpu(arch.hart_id, i) == hart_id) {
			pcpu_id = i;
			break;
		}
	}

	return pcpu_id;
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
