/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static uint32_t notification_irq = IRQ_INVALID;

static uint64_t smp_call_mask = 0UL;

/* run in interrupt context */
static void kick_notification(__unused uint32_t irq, __unused void *data)
{
	/* Notification vector is used to kick taget cpu out of non-root mode.
	 * And it also serves for smp call.
	 */
	uint16_t pcpu_id = get_cpu_id();

	if (bitmap_test(pcpu_id, &smp_call_mask)) {
		struct smp_call_info_data *smp_call =
			&per_cpu(smp_call_info, pcpu_id);

		if (smp_call->func != NULL) {
			smp_call->func(smp_call->data);
		}
		bitmap_clear_nolock(pcpu_id, &smp_call_mask);
	}
}

void smp_call_function(uint64_t mask, smp_call_func_t func, void *data)
{
	uint16_t pcpu_id;
	struct smp_call_info_data *smp_call;

	/* wait for previous smp call complete, which may run on other cpus */
	while (atomic_cmpxchg64(&smp_call_mask, 0UL, mask & INVALID_BIT_INDEX)
									!= 0UL);
	pcpu_id = ffs64(mask);
	while (pcpu_id != INVALID_BIT_INDEX) {
		bitmap_clear_nolock(pcpu_id, &mask);
		if (bitmap_test(pcpu_id, &pcpu_active_bitmap)) {
			smp_call = &per_cpu(smp_call_info, pcpu_id);
			smp_call->func = func;
			smp_call->data = data;
		} else {
			/* pcpu is not in active, print error */
			pr_err("pcpu_id %d not in active!", pcpu_id);
			bitmap_clear_nolock(pcpu_id, &smp_call_mask);
		}
		pcpu_id = ffs64(mask);
	}
	send_dest_ipi((uint32_t)smp_call_mask, VECTOR_NOTIFY_VCPU,
				INTR_LAPIC_ICR_LOGICAL);
	/* wait for current smp call complete */
	wait_sync_change(&smp_call_mask, 0UL);
}

static int request_notification_irq(irq_action_t func, void *data)
{
	int32_t retval;

	if (notification_irq != IRQ_INVALID) {
		pr_info("%s, Notification vector already allocated on this CPU",
				__func__);
		return -EBUSY;
	}

	/* all cpu register the same notification vector */
	retval = request_irq(NOTIFY_IRQ, func, data, IRQF_NONE);
	if (retval < 0) {
		pr_err("Failed to add notify isr");
		return -ENODEV;
	}

	notification_irq = (uint32_t)retval;

	return 0;
}

void setup_notification(void)
{
	uint16_t cpu = get_cpu_id();

	if (cpu > 0U) {
		return;
	}

	/* support IPI notification, VM0 will register all CPU */
	if (request_notification_irq(kick_notification, NULL) < 0) {
		pr_err("Failed to setup notification");
		return;
	}

	dev_dbg(ACRN_DBG_PTIRQ, "NOTIFY: irq[%d] setup vector %x",
		notification_irq, irq_to_vector(notification_irq));
}

static void posted_intr_notification(__unused uint32_t irq, __unused void *data)
{
	/* Dummy IRQ handler for case that Posted-Interrupt Notification
	 * is sent to vCPU in root mode(isn't running),interrupt will be
	 * picked up in next vmentry,do nothine here.
	 */
}

/*pre-conditon: be called only by BSP initialization proccess*/
void setup_posted_intr_notification(void)
{
	if (request_irq(POSTED_INTR_NOTIFY_IRQ,
			posted_intr_notification,
			NULL, IRQF_NONE) < 0) {
		pr_err("Failed to setup posted-intr notification");
		return;
	}
}
