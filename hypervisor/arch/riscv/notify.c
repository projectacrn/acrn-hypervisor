/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <types.h>
#include <irq.h>
#include <asm/sbi.h>
#include <asm/trap.h>
#include <schedule.h>

void arch_init_smp_call(void)
{
	uint32_t acrn_irq;

	if (get_pcpu_id() == BSP_CPU_ID) {
		acrn_irq = riscv_domain_get_acrn_irq(RISCV_IRQD_CPU, TRAP_CAUSE_IRQ_S_SOFT);

		if ((acrn_irq == IRQ_INVALID) || ((request_irq(acrn_irq, s_sw_irq_handler, NULL, IRQF_NONE) < 0))) {
			pr_err("Software interrupt IRQ setup failed \n");
		}
	}
}

void arch_smp_call_kick_pcpu(uint16_t pcpu_id)
{
	send_single_ipi(pcpu_id, IPI_NOTIFY_CPU);
}

void arch_send_reschedule_request(uint16_t pcpu_id)
{
	send_single_ipi(pcpu_id, IPI_NOTIFY_CPU);
}
