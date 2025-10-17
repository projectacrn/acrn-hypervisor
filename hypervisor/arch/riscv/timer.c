/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <softirq.h>
#include <timer.h>
#include <irq.h>
#include <asm/sbi.h>
#include <asm/qemu.h>
#include <asm/cpu.h>
#include <asm/csr.h>
#include <asm/trap.h>

#define HOST_CPUFREQ	10000000
#define STOP_TIMER	0xFFFFFFFFFFFFFFFF

static void timer_irq_handler(__unused uint32_t irq, __unused void *data)
{
	arch_set_timer_count(STOP_TIMER);
	fire_softirq(SOFTIRQ_TIMER);
}

void arch_init_timer(void)
{
	uint32_t acrn_irq;

	if (get_pcpu_id() == BSP_CPU_ID) {
		acrn_irq = riscv_domain_get_acrn_irq(RISCV_IRQD_CPU, TRAP_CAUSE_IRQ_S_TIMER);

		if ((acrn_irq == IRQ_INVALID) || ((request_irq(acrn_irq, timer_irq_handler, NULL, IRQF_NONE) < 0))) {
			pr_err("Timer IRQ setup failed \n");
		}
	}
}

/* FIXME:
 * Such short arch_xxx function need be moved into header file with
 * static inline prefix.
 */
uint64_t arch_cpu_ticks(void)
{
	uint64_t tick;
	asm volatile (
		"rdtime %0":"=r"(tick):: "memory");
	return tick;
}

uint32_t arch_cpu_tickrate(void)
{
	return HOST_CPUFREQ / 1000;
}

void arch_set_timer_count(uint64_t timeout)
{
#ifdef CONFIG_SSTC
	cpu_csr_write(CSR_STIMECMP, timeout);
#else
	sbi_set_timer(timeout);
#endif
}
