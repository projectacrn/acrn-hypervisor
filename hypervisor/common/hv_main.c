/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <asm/guest/vm_reset.h>
#include <asm/guest/vmcs.h>
#include <asm/guest/vmexit.h>
#include <asm/guest/virq.h>
#include <schedule.h>
#include <profiling.h>
#include <sprintf.h>
#include <trace.h>
#include <logmsg.h>
#include <per_cpu.h>

void vcpu_thread(struct thread_object *obj)
{
	struct acrn_vcpu *vcpu = container_of(obj, struct acrn_vcpu, thread_obj);
	int32_t ret = 0;

	do {
		if (!is_lapic_pt_enabled(vcpu)) {
			CPU_IRQ_DISABLE_ON_CONFIG();
		}

		/* Don't open interrupt window between here and vmentry */
		if (need_reschedule(pcpuid_from_vcpu(vcpu))) {
			schedule();
		}

		/* Check and process pending requests(including interrupt) */
		ret = acrn_handle_pending_request(vcpu);
		if (ret < 0) {
			pr_fatal("vcpu handling pending request fail");
			get_vm_lock(vcpu->vm);
			zombie_vcpu(vcpu, VCPU_ZOMBIE);
			put_vm_lock(vcpu->vm);
			/* Fatal error happened (triple fault). Stop the vcpu running. */
			continue;
		}

		reset_event(&vcpu->events[VCPU_EVENT_VIRTUAL_INTERRUPT]);
		profiling_vmenter_handler(vcpu);

		TRACE_2L(TRACE_VM_ENTER, 0UL, 0UL);
		ret = run_vcpu(vcpu);
		if (ret != 0) {
			pr_fatal("vcpu resume failed");
			get_vm_lock(vcpu->vm);
			zombie_vcpu(vcpu, VCPU_ZOMBIE);
			put_vm_lock(vcpu->vm);
			/* Fatal error happened (resume vcpu failed). Stop the vcpu running. */
			continue;
		}
		TRACE_2L(TRACE_VM_EXIT, vcpu->arch.exit_reason, vcpu_get_rip(vcpu));

		profiling_pre_vmexit_handler(vcpu);

		if (!is_lapic_pt_enabled(vcpu)) {
			CPU_IRQ_ENABLE_ON_CONFIG();
		}
		/* Dispatch handler */
		ret = vmexit_handler(vcpu);
		if (ret < 0) {
			pr_fatal("dispatch VM exit handler failed for reason"
				" %d, ret = %d!", vcpu->arch.exit_reason, ret);
			vcpu_inject_gp(vcpu, 0U);
			continue;
		}

		profiling_post_vmexit_handler(vcpu);
	} while (1);
}

void default_idle(__unused struct thread_object *obj)
{
	uint16_t pcpu_id = get_pcpu_id();

	while (1) {
		if (need_reschedule(pcpu_id)) {
			schedule();
		} else if (need_offline(pcpu_id)) {
			cpu_dead();
		} else if (need_shutdown_vm(pcpu_id)) {
			shutdown_vm_from_idle(pcpu_id);
		} else {
			cpu_do_idle();
		}
	}
}

void run_idle_thread(void)
{
	uint16_t pcpu_id = get_pcpu_id();
	struct thread_object *idle = &per_cpu(idle, pcpu_id);
	struct sched_params idle_params = {0};
	char idle_name[16];

	snprintf(idle_name, 16U, "idle%hu", pcpu_id);
	(void)strncpy_s(idle->name, 16U, idle_name, 16U);
	idle->pcpu_id = pcpu_id;
	idle->thread_entry = default_idle;
	idle->switch_out = NULL;
	idle->switch_in = NULL;
	idle_params.prio = PRIO_IDLE;
	init_thread_data(idle, &idle_params);

	run_thread(idle);

	/* Control should not come here */
	cpu_dead();
}
