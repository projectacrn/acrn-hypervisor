/*
 * Copyright (C) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <softirq.h>
#include <irq.h>
#include <logmsg.h>
#include <cpu.h>
#include <asm/msr.h>
#include <asm/irq.h>
#include <asm/apicreg.h>
#include <hw/hw_thermal.h>

/* run in interrupt context */
static void thermal_irq_handler(__unused uint32_t irq, __unused void *data)
{
	fire_softirq(SOFTIRQ_THERMAL);
}

void init_hw_thermal(void)
{
	int32_t retval = 0;

	if (get_pcpu_id() == BSP_CPU_ID) {
		retval = request_irq(THERMAL_IRQ, thermal_irq_handler, NULL, IRQF_NONE);
		if (retval < 0) {
			pr_err("Thermal irq setup failed\n");
		}
	}

	if (retval >= 0) {
		uint32_t val = THERMAL_VECTOR;

		msr_write(MSR_IA32_EXT_APIC_LVT_THERMAL, val);
	}
}
