/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

void disable_softirq(uint16_t cpu_id)
{
	bitmap_clear(SOFTIRQ_ATOMIC, &per_cpu(softirq_pending, cpu_id));
}

void enable_softirq(uint16_t cpu_id)
{
	bitmap_set(SOFTIRQ_ATOMIC, &per_cpu(softirq_pending, cpu_id));
}

void init_softirq(void)
{
	uint16_t pcpu_id;

	for (pcpu_id = 0U; pcpu_id < phys_cpu_num; pcpu_id++) {
		per_cpu(softirq_pending, pcpu_id) = 0UL;
		bitmap_set(SOFTIRQ_ATOMIC, &per_cpu(softirq_pending, pcpu_id));
	}
}

void raise_softirq(int softirq_id)
{
	uint16_t cpu_id = get_cpu_id();
	uint64_t *bitmap = &per_cpu(softirq_pending, cpu_id);

	if (cpu_id >= phys_cpu_num)
		return;

	bitmap_set(softirq_id, bitmap);
}

void exec_softirq(void)
{
	uint16_t cpu_id = get_cpu_id();
	volatile uint64_t *bitmap = &per_cpu(softirq_pending, cpu_id);

	uint16_t softirq_id;

	if (cpu_id >= phys_cpu_num)
		return;

	if (((*bitmap) & SOFTIRQ_MASK) == 0UL)
		return;

	/* Disable softirq
	 * SOFTIRQ_ATOMIC bit = 0 means softirq already in execution
	 */
	if (!bitmap_test_and_clear(SOFTIRQ_ATOMIC, bitmap))
		return;

again:
	CPU_IRQ_ENABLE();

	while (1) {
		softirq_id = ffs64(*bitmap);
		if ((softirq_id == INVALID_BIT_INDEX) || (softirq_id >= SOFTIRQ_MAX))
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

	CPU_IRQ_DISABLE();

	if (((*bitmap) & SOFTIRQ_MASK) != 0U)
		goto again;

	enable_softirq(cpu_id);
}

