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
#include <asm/timer.h>
#include <asm/sbi.h>
#include <asm/qemu.h>
#include <asm/cpu.h>

#define HOST_CPUFREQ	10000000
#define STOP_TIMER	0xFFFFFFFFFFFFFFFF

void timer_irq_handler(void)
{
	arch_set_timer_count(STOP_TIMER);
	fire_softirq(SOFTIRQ_TIMER);
}

void arch_init_timer(void)
{
	return;
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
	cpu_csr_write(stimecmp, timeout);
#else
	sbi_set_timer(timeout);
#endif
}
