/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <vm_reset.h>
#include <vmcs.h>
#include <vmexit.h>
#include <irq.h>
#include <schedule.h>
#include <profiling.h>
#include <sprintf.h>
#include <trace.h>
#include <logmsg.h>
#include <shell.h>

void vcpu_thread(struct thread_object *obj)
{
#ifdef HV_DEBUG
	uint64_t vmexit_begin = 0UL, vmexit_end = 0UL;
#endif
	struct acrn_vcpu *vcpu = container_of(obj, struct acrn_vcpu, thread_obj);
	uint32_t basic_exit_reason = 0U;
	int32_t ret = 0;

	do {
		if (!is_lapic_pt_enabled(vcpu)) {
			CPU_IRQ_DISABLE();
		}

		/* Don't open interrupt window between here and vmentry */
		if (need_reschedule(pcpuid_from_vcpu(vcpu))) {
			schedule();
		}

		/* Check and process pending requests(including interrupt) */
		ret = acrn_handle_pending_request(vcpu);
		if (ret < 0) {
			pr_fatal("vcpu handling pending request fail");
			zombie_vcpu(vcpu, VCPU_ZOMBIE);
			/* Fatal error happened (triple fault). Stop the vcpu running. */
			continue;
		}

		reset_event(&vcpu->events[VCPU_EVENT_VIRTUAL_INTERRUPT]);
		profiling_vmenter_handler(vcpu);

#ifdef HV_DEBUG
		if (is_vmexit_sample_enabled()) {
			vmexit_end = rdtsc();
			if (vmexit_begin != 0UL) {
				uint64_t delta = vmexit_end - vmexit_begin;
				uint32_t us = (uint32_t)ticks_to_us(delta);
				uint16_t fls = (uint16_t)(fls32(us) + 1); /* to avoid us = 0 case, then fls=0xFFFF */
				uint16_t index = 0;

				if (fls >= MAX_VMEXIT_LEVEL) {
					index = MAX_VMEXIT_LEVEL - 1;
				} else if (fls > 0) { //if fls == 0, it means the us = 0
					index = fls - 1;
				}

				get_cpu_var(vmexit_cnt)[basic_exit_reason][index]++;
				get_cpu_var(vmexit_time)[basic_exit_reason][0] += delta;

				vcpu->vmexit_cnt[basic_exit_reason][index]++;
				vcpu->vmexit_time[basic_exit_reason][0] += delta;

				if (us > get_cpu_var(vmexit_time)[basic_exit_reason][1]) {
					get_cpu_var(vmexit_time)[basic_exit_reason][1] = us;
				}

				if (us > vcpu->vmexit_time[basic_exit_reason][1]) {
					vcpu->vmexit_time[basic_exit_reason][1] = us;
				}
			}
		}
#endif
		TRACE_2L(TRACE_VM_ENTER, 0UL, 0UL);
		ret = run_vcpu(vcpu);
		if (ret != 0) {
			pr_fatal("vcpu resume failed");
			zombie_vcpu(vcpu, VCPU_ZOMBIE);
			/* Fatal error happened (resume vcpu failed). Stop the vcpu running. */
			continue;
		}
		basic_exit_reason = vcpu->arch.exit_reason & 0xFFFFU;
		TRACE_2L(TRACE_VM_EXIT, basic_exit_reason, vcpu_get_rip(vcpu));

#ifdef HV_DEBUG
		if (is_vmexit_sample_enabled()) {
			vmexit_begin = rdtsc();
			get_cpu_var(vmexit_cnt)[basic_exit_reason][TOTAL_ARRAY_LEVEL - 1]++;
			vcpu->vmexit_cnt[basic_exit_reason][TOTAL_ARRAY_LEVEL - 1]++;
		}
#endif
		vcpu->arch.nrexits++;

		profiling_pre_vmexit_handler(vcpu);

		if (!is_lapic_pt_enabled(vcpu)) {
			CPU_IRQ_ENABLE();
		}
		/* Dispatch handler */
		ret = vmexit_handler(vcpu);
		if (ret < 0) {
			pr_fatal("dispatch VM exit handler failed for reason"
				" %d, ret = %d!", basic_exit_reason, ret);
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
			CPU_IRQ_ENABLE();
			cpu_do_idle();
			CPU_IRQ_DISABLE();
		}
	}
}

void run_idle_thread(void)
{
	uint16_t pcpu_id = get_pcpu_id();
	struct thread_object *idle = &per_cpu(idle, pcpu_id);
	char idle_name[16];

	snprintf(idle_name, 16U, "idle%hu", pcpu_id);
	(void)strncpy_s(idle->name, 16U, idle_name, 16U);
	idle->pcpu_id = pcpu_id;
	idle->thread_entry = default_idle;
	idle->switch_out = NULL;
	idle->switch_in = NULL;

	run_thread(idle);

	/* Control should not come here */
	cpu_dead();
}
