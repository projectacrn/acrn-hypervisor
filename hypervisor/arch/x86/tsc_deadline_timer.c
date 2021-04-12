/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <softirq.h>
#include <irq.h>
#include <logmsg.h>
#include <asm/cpu.h>
#include <asm/msr.h>
#include <asm/irq.h>
#include <asm/apicreg.h>
#include <hw/hw_timer.h>

/* run in interrupt context */
static void timer_expired_handler(__unused uint32_t irq, __unused void *data)
{
	fire_softirq(SOFTIRQ_TIMER);
}

void set_hw_timeout(uint64_t timeout)
{
	msr_write(MSR_IA32_TSC_DEADLINE, timeout);
}

void init_hw_timer(void)
{
	int32_t retval = 0;

	if (get_pcpu_id() == BSP_CPU_ID) {
		retval = request_irq(TIMER_IRQ, (irq_action_t)timer_expired_handler, NULL, IRQF_NONE);
		if (retval < 0) {
			pr_err("Timer setup failed");
		}
	}

	if (retval >= 0) {
		uint32_t val = TIMER_VECTOR;
		val |= APIC_LVTT_TM_TSCDLT; /* TSC deadline and unmask */
		msr_write(MSR_IA32_EXT_APIC_LVT_TIMER, val);
		/* SDM 10.5.4.1: In x2APIC mode, the processor ensures the
                   ordering of this write and any subsequent WRMSR to the
                   deadline; no fencing is required. */

		/* disarm timer */
		msr_write(MSR_IA32_TSC_DEADLINE, 0UL);
	}
}
