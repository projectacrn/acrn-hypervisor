/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static uint32_t notification_irq = IRQ_INVALID;

/* run in interrupt context */
static int kick_notification(__unused uint32_t irq, __unused void *data)
{
	/* Notification vector does not require handling here, it's just used
	 * to kick taget cpu out of non-root mode.
	 */
	return 0;
}

static int request_notification_irq(irq_action_t func, void *data,
				const char *name)
{
	int32_t retval;

	if (notification_irq != IRQ_INVALID) {
		pr_info("%s, Notification vector already allocated on this CPU",
				__func__);
		return -EBUSY;
	}

	/* all cpu register the same notification vector */
	retval = pri_register_handler(NOTIFY_IRQ, func, data, name);
	if (retval < 0) {
		pr_err("Failed to add notify isr");
		return -ENODEV;
	}

	notification_irq = (uint32_t)retval;

	return 0;
}

void setup_notification(void)
{
	uint16_t cpu;
	char name[32] = {0};

	cpu = get_cpu_id();
	if (cpu > 0U) {
		return;
	}

	/* support IPI notification, VM0 will register all CPU */
	snprintf(name, 32, "NOTIFY_ISR%d", cpu);
	if (request_notification_irq(kick_notification, NULL, name) < 0) {
		pr_err("Failed to setup notification");
		return;
	}

	dev_dbg(ACRN_DBG_PTIRQ, "NOTIFY: irq[%d] setup vector %x",
		notification_irq, irq_to_vector(notification_irq));
}

static void cleanup_notification(void)
{
	if (notification_irq != IRQ_INVALID) {
		unregister_handler_common(notification_irq);
	}
	notification_irq = IRQ_INVALID;
}
