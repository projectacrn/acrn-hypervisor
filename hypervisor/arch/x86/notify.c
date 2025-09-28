/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <atomic.h>
#include <bits.h>
#include <asm/irq.h>
#include <asm/cpu.h>
#include <asm/per_cpu.h>
#include <asm/lapic.h>
#include <asm/guest/vm.h>
#include <asm/guest/virq.h>
#include <common/notify.h>
#include <schedule.h>
#include <irq.h>
#include <logmsg.h>

static uint32_t notification_irq = IRQ_INVALID;

static int32_t request_notification_irq(irq_action_t func, void *data)
{
	int32_t retval;

	if (notification_irq != IRQ_INVALID) {
		pr_info("%s, Notification vector already allocated on this CPU", __func__);
		retval = -EBUSY;
	} else {
		/* all cpu register the same notification vector */
		retval = request_irq(NOTIFY_VCPU_IRQ, func, data, IRQF_NONE);
		if (retval < 0) {
			pr_err("Failed to add notify isr");
			retval = -ENODEV;
		} else {
			notification_irq = (uint32_t)retval;
		}
	}

	return retval;
}

/*
 * @pre be called only by BSP initialization process
 */
static void setup_notification(void)
{
	/* support IPI notification, Service VM will register all CPU */
	if (request_notification_irq(kick_notification, NULL) < 0) {
		pr_err("Failed to setup notification");
	}

	dev_dbg(DBG_LEVEL_PTIRQ, "NOTIFY: irq[%d] setup vector %x",
		notification_irq, irq_to_vector(notification_irq));
}

/*
 * posted interrupt handler
 * @pre (irq - POSTED_INTR_IRQ) < CONFIG_MAX_VM_NUM
 */
static void handle_pi_notification(uint32_t irq, __unused void *data)
{
	uint32_t vcpu_index = irq - POSTED_INTR_IRQ;

	ASSERT(vcpu_index < CONFIG_MAX_VM_NUM, "");
	vcpu_handle_pi_notification(vcpu_index);
}

/*pre-condition: be called only by BSP initialization proccess*/
static void setup_pi_notification(void)
{
	uint32_t i;

	for (i = 0U; i < CONFIG_MAX_VM_NUM; i++) {
		if (request_irq(POSTED_INTR_IRQ + i, handle_pi_notification, NULL, IRQF_NONE) < 0) {
			pr_err("Failed to setup pi notification");
			break;
		}
	}
}

void arch_init_smp_call(void)
{
	setup_notification();
	setup_pi_notification();
}

void arch_smp_call_kick_pcpu(uint16_t pcpu_id)
{
	struct acrn_vcpu *vcpu = get_ever_run_vcpu(pcpu_id);

	if ((vcpu != NULL) && (is_lapic_pt_enabled(vcpu))) {
		vcpu_make_request(vcpu, ACRN_REQUEST_SMP_CALL);
	} else {
		send_single_ipi(pcpu_id, NOTIFY_VCPU_VECTOR);
	}
}

void arch_send_reschedule_request(uint16_t pcpu_id)
{
	kick_pcpu(pcpu_id);
}
