/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static DEFINE_CPU_DATA(uint64_t, softirq_pending);

void disable_softirq(int cpu_id)
{
	bitmap_clear(SOFTIRQ_ATOMIC, &per_cpu(softirq_pending, cpu_id));
}

void enable_softirq(int cpu_id)
{
	bitmap_set(SOFTIRQ_ATOMIC, &per_cpu(softirq_pending, cpu_id));
}

void init_softirq(void)
{
	int cpu_id;

	for (cpu_id = 0; cpu_id < phy_cpu_num; cpu_id++) {
		per_cpu(softirq_pending, cpu_id) = 0;
		bitmap_set(SOFTIRQ_ATOMIC, &per_cpu(softirq_pending, cpu_id));
	}
}

void raise_softirq(int softirq_id)
{
	int cpu_id = get_cpu_id();
	uint64_t *bitmap = &per_cpu(softirq_pending, cpu_id);

	if (cpu_id >= phy_cpu_num)
		return;

	bitmap_set(softirq_id, bitmap);
}

void exec_softirq(void)
{
	int cpu_id = get_cpu_id();
	uint64_t *bitmap = &per_cpu(softirq_pending, cpu_id);

	uint64_t rflag;
	int softirq_id;

	if (cpu_id >= phy_cpu_num)
		return;

	/* Disable softirq
	 * SOFTIRQ_ATOMIC bit = 0 means softirq already in execution
	 */
	if (!bitmap_test_and_clear(SOFTIRQ_ATOMIC, bitmap))
		return;

	if (((*bitmap) & SOFTIRQ_MASK) == 0UL)
		goto ENABLE_AND_EXIT;

	/* check if we are in interrupt context */
	CPU_RFLAGS_SAVE(&rflag);
	if (!(rflag & (1<<9)))
		goto ENABLE_AND_EXIT;

	while (1) {
		softirq_id = ffs64(*bitmap);
		if ((softirq_id < 0) || (softirq_id >= SOFTIRQ_MAX))
			break;

		bitmap_clear(softirq_id, bitmap);

		switch (softirq_id) {
		case SOFTIRQ_TIMER:
			timer_softirq(cpu_id);
			break;
		case SOFTIRQ_DEV_ASSIGN:
			ptdev_softirq(cpu_id);
			break;
		default:
			break;

		}
	}

ENABLE_AND_EXIT:
	enable_softirq(cpu_id);
}

