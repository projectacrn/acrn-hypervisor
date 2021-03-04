/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <x86/lib/bits.h>
#include <x86/lib/atomic.h>
#include <x86/irq.h>
#include <x86/cpu.h>
#include <x86/per_cpu.h>
#include <x86/lapic.h>
#include <x86/guest/vm.h>

static uint32_t notification_irq = IRQ_INVALID;

static uint64_t smp_call_mask = 0UL;

/* run in interrupt context */
static void kick_notification(__unused uint32_t irq, __unused void *data)
{
	/* Notification vector is used to kick taget cpu out of non-root mode.
	 * And it also serves for smp call.
	 */
	uint16_t pcpu_id = get_pcpu_id();

	if (bitmap_test(pcpu_id, &smp_call_mask)) {
		struct smp_call_info_data *smp_call =
			&per_cpu(smp_call_info, pcpu_id);

		if (smp_call->func != NULL) {
			smp_call->func(smp_call->data);
		}
		bitmap_clear_lock(pcpu_id, &smp_call_mask);
	}
}

void smp_call_function(uint64_t mask, smp_call_func_t func, void *data)
{
	uint16_t pcpu_id;
	struct smp_call_info_data *smp_call;

	/* wait for previous smp call complete, which may run on other cpus */
	while (atomic_cmpxchg64(&smp_call_mask, 0UL, mask) != 0UL);
	pcpu_id = ffs64(mask);
	while (pcpu_id < MAX_PCPU_NUM) {
		bitmap_clear_nolock(pcpu_id, &mask);
		if (is_pcpu_active(pcpu_id)) {
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
	send_dest_ipi_mask((uint32_t)smp_call_mask, NOTIFY_VCPU_VECTOR);
	/* wait for current smp call complete */
	wait_sync_change(&smp_call_mask, 0UL);
}

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
void setup_notification(void)
{
	/* support IPI notification, SOS_VM will register all CPU */
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
void setup_pi_notification(void)
{
	uint32_t i;

	for (i = 0U; i < CONFIG_MAX_VM_NUM; i++) {
		if (request_irq(POSTED_INTR_IRQ + i, handle_pi_notification, NULL, IRQF_NONE) < 0) {
			pr_err("Failed to setup pi notification");
			break;
		}
	}
}

/**
 * @brief Check if the NMI is for notification purpose
 *
 * @return true, if the NMI is triggered for notifying vCPU
 * @return false, if the NMI is triggered for other purpose
 */
bool is_notification_nmi(const struct acrn_vm *vm)
{
	bool ret;

	/*
	 * Currently, ACRN doesn't support vNMI well and there is no well-designed
	 * way to check if the NMI is for notification or not. Here we take all the
	 * NMIs as notification NMI for lapic-pt VMs temporarily.
	 *
	 * TODO: Add a way to check the NMI is for notification or not in order to support vNMI.
	 */
	ret = is_lapic_pt_configured(vm);

	return ret;
}
