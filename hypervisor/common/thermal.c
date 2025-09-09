/*
 * Copyright (C) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <softirq.h>
#include <trace.h>
#include <asm/guest/virq.h>
#include <hw/hw_thermal.h>
#include <per_cpu.h>

static void thermal_softirq(uint16_t pcpu_id)
{
	struct acrn_vcpu *vcpu;
	uint32_t idx;

	for (idx = 0; idx < CONFIG_MAX_VM_NUM; idx++) {
		vcpu = per_cpu(vcpu_array, pcpu_id)[idx];
		if (vcpu != NULL) {
			vcpu_inject_thermal_interrupt(vcpu);
		}
	}
}

void thermal_init(void)
{
	uint16_t pcpu_id = get_pcpu_id();

	if (pcpu_id == BSP_CPU_ID) {
		register_softirq(SOFTIRQ_THERMAL, thermal_softirq);
	}

	init_hw_thermal();
}
