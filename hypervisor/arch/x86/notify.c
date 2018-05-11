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
#include <hv_debug.h>
#include <irq.h>

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
	int irq = -1; /* system allocate */
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
	if (notification_node)
		unregister_handler_common(notification_node);
	notification_node = NULL;
}
