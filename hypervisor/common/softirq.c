/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <x86/lib/bits.h>
#include <x86/cpu.h>
#include <x86/per_cpu.h>
#include <softirq.h>

static softirq_handler softirq_handlers[NR_SOFTIRQS];

void init_softirq(void)
{
}

/*
 * @pre: nr will not equal or large than NR_SOFTIRQS
 */
void register_softirq(uint16_t nr, softirq_handler handler)
{
	softirq_handlers[nr] = handler;
}

/*
 * @pre: nr will not equal or large than NR_SOFTIRQS
 */
void fire_softirq(uint16_t nr)
{
	bitmap_set_lock(nr, &per_cpu(softirq_pending, get_pcpu_id()));
}

static void do_softirq_internal(uint16_t cpu_id)
{
	volatile uint64_t *softirq_pending_bitmap =
			&per_cpu(softirq_pending, cpu_id);
	uint16_t nr = ffs64(*softirq_pending_bitmap);

	while (nr < NR_SOFTIRQS) {
		bitmap_clear_lock(nr, softirq_pending_bitmap);
		(*softirq_handlers[nr])(cpu_id);
		nr = ffs64(*softirq_pending_bitmap);
	}
}

/*
 * @pre: this function will only be called with irq disabled
 */
void do_softirq(void)
{
	uint16_t cpu_id = get_pcpu_id();

	if (per_cpu(softirq_servicing, cpu_id) == 0U) {
		per_cpu(softirq_servicing, cpu_id) = 1U;

		CPU_IRQ_ENABLE();
		do_softirq_internal(cpu_id);
		CPU_IRQ_DISABLE();

		do_softirq_internal(cpu_id);
		per_cpu(softirq_servicing, cpu_id) = 0U;
	}
}
