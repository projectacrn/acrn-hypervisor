/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>

static DEFINE_CPU_DATA(uint64_t, softirq_pending);

void disable_softirq(int cpu_id)
{
	bitmap_clr(SOFTIRQ_ATOMIC, &per_cpu(softirq_pending, cpu_id));
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
		softirq_id = bitmap_ffs(bitmap);
		if ((softirq_id < 0) || (softirq_id >= SOFTIRQ_MAX))
			break;

		bitmap_clr(softirq_id, bitmap);

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

