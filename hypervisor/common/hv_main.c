/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>
#include <softirq.h>

static void run_vcpu_pre_work(struct acrn_vcpu *vcpu)
{
	uint64_t *pending_pre_work = &vcpu->pending_pre_work;

	if (bitmap_test_and_clear_lock(ACRN_VCPU_MMIO_COMPLETE, pending_pre_work)) {
		dm_emulate_mmio_post(vcpu);
	}
}

void vcpu_thread(struct sched_object *obj)
{
	struct acrn_vcpu *vcpu = list_entry(obj, struct acrn_vcpu, sched_obj);
	uint32_t basic_exit_reason = 0U;
	int32_t ret = 0;

	/* If vcpu is not launched, we need to do init_vmcs first */
	if (!vcpu->launched) {
		init_vmcs(vcpu);
	}

	run_vcpu_pre_work(vcpu);

	do {
		/* handle pending softirq when irq enable*/
		do_softirq();
		CPU_IRQ_DISABLE();
		/* handle risk softirq when disabling irq*/
		do_softirq();

		/* Check and process pending requests(including interrupt) */
		ret = acrn_handle_pending_request(vcpu);
		if (ret < 0) {
			pr_fatal("vcpu handling pending request fail");
			pause_vcpu(vcpu, VCPU_ZOMBIE);
			continue;
		}

		if (need_reschedule(vcpu->pcpu_id)) {
			/*
			 * In extrem case, schedule() could return. Which
			 * means the vcpu resume happens before schedule()
			 * triggered by vcpu suspend. In this case, we need
			 * to do pre work and continue vcpu loop after
			 * schedule() is return.
			 */
			schedule();
			run_vcpu_pre_work(vcpu);
			continue;
		}

		TRACE_2L(TRACE_VM_ENTER, 0UL, 0UL);

		profiling_vmenter_handler(vcpu);

		ret = run_vcpu(vcpu);
		if (ret != 0) {
			pr_fatal("vcpu resume failed");
			pause_vcpu(vcpu, VCPU_ZOMBIE);
			continue;
		}

		vcpu->arch.nrexits++;

		profiling_pre_vmexit_handler(vcpu);

		CPU_IRQ_ENABLE();
		/* Dispatch handler */
		ret = vmexit_handler(vcpu);
		basic_exit_reason = vcpu->arch.exit_reason & 0xFFFFU;
		if (ret < 0) {
			pr_fatal("dispatch VM exit handler failed for reason"
				" %d, ret = %d!", basic_exit_reason, ret);
			vcpu_inject_gp(vcpu, 0U);
			continue;
		}

		TRACE_2L(TRACE_VM_EXIT, basic_exit_reason, vcpu_get_rip(vcpu));

		profiling_post_vmexit_handler(vcpu);
	} while (1);
}

void default_idle(__unused struct sched_object *obj)
{
	uint16_t pcpu_id = get_cpu_id();

	while (1) {
		if (need_reschedule(pcpu_id)) {
			schedule();
		} else if (need_offline(pcpu_id) != 0) {
			cpu_dead();
		} else {
			CPU_IRQ_ENABLE();
			handle_complete_ioreq(pcpu_id);
			cpu_do_idle();
			CPU_IRQ_DISABLE();
		}
	}
}
