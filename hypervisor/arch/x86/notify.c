/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static struct dev_handler_node *notification_node;

/* run in interrupt context */
static int kick_notification(__unused int irq, __unused void *data)
{
	/* Notification vector does not require handling here, it's just used
	 * to kick taget cpu out of non-root mode.
	 */
	return 0;
}

static int request_notification_irq(dev_handler_t func, void *data,
				const char *name)
{
	uint32_t irq = IRQ_INVALID; /* system allocate */
	struct dev_handler_node *node = NULL;

	if (notification_node != NULL) {
		pr_info("%s, Notification vector already allocated on this CPU",
				__func__);
		return -EBUSY;
	}

	/* all cpu register the same notification vector */
	node = pri_register_handler(irq, VECTOR_NOTIFY_VCPU, func, data, name);
	if (node == NULL) {
		pr_err("Failed to add notify isr");
		return -1;
	}
	update_irq_handler(dev_to_irq(node), quick_handler_nolock);
	notification_node = node;
	return 0;
}

void setup_notification(void)
{
	int cpu;
	char name[32] = {0};

	cpu = get_cpu_id();
	if (cpu > 0)
		return;

	/* support IPI notification, VM0 will register all CPU */
	snprintf(name, 32, "NOTIFY_ISR%d", cpu);
	if (request_notification_irq(kick_notification, NULL, name) < 0) {
		pr_err("Failed to setup notification");
		return;
	}

	dev_dbg(ACRN_DBG_PTIRQ, "NOTIFY: irq[%d] setup vector %x",
		dev_to_irq(notification_node),
		dev_to_vector(notification_node));
}

void cleanup_notification(void)
{
	if (notification_node != NULL)
		unregister_handler_common(notification_node);
	notification_node = NULL;
}
