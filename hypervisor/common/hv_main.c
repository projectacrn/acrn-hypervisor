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

void vcpu_thread(struct acrn_vcpu *vcpu)
{
#ifdef HV_DEBUG
	uint64_t vmexit_begin = 0UL, vmexit_end = 0UL;
#endif
	uint32_t basic_exit_reason = 0U;
	uint64_t tsc_aux_hyp_cpu = (uint64_t) vcpu->pcpu_id;
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

		if (need_reschedule(vcpu->pcpu_id) != 0) {
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

#ifdef HV_DEBUG
		vmexit_end = rdtsc();
		if (vmexit_begin != 0UL) {
			per_cpu(vmexit_time, vcpu->pcpu_id)[basic_exit_reason]
				+= (vmexit_end - vmexit_begin);
		}
#endif
		TRACE_2L(TRACE_VM_ENTER, 0UL, 0UL);

		profiling_vmenter_handler(vcpu);

		/* Restore guest TSC_AUX */
		if (vcpu->launched) {
			cpu_msr_write(MSR_IA32_TSC_AUX,
					vcpu->msr_tsc_aux_guest);
		}

		ret = run_vcpu(vcpu);
		if (ret != 0) {
			pr_fatal("vcpu resume failed");
			pause_vcpu(vcpu, VCPU_ZOMBIE);
			continue;
		}

#ifdef HV_DEBUG
		vmexit_begin = rdtsc();
#endif

		vcpu->arch.nrexits++;
		/* Save guest TSC_AUX */
		cpu_msr_read(MSR_IA32_TSC_AUX, &vcpu->msr_tsc_aux_guest);
		/* Restore native TSC_AUX */
		cpu_msr_write(MSR_IA32_TSC_AUX, tsc_aux_hyp_cpu);

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

#ifdef HV_DEBUG
		per_cpu(vmexit_cnt, vcpu->pcpu_id)[basic_exit_reason]++;
#endif

		TRACE_2L(TRACE_VM_EXIT, basic_exit_reason, vcpu_get_rip(vcpu));

		profiling_vmexit_handler(vcpu, basic_exit_reason);
	} while (1);
}

#ifdef HV_DEBUG
void get_vmexit_profile(char *str_arg, size_t str_max)
{
	char *str = str_arg;
	uint16_t cpu, i;
	size_t len, size = str_max;

	len = snprintf(str, size, "\r\nNow(us) = %16lld\r\n", ticks_to_us(rdtsc()));
	if (len >= size) {
		goto overflow;
	}
	size -= len;
	str += len;

	len = snprintf(str, size, "\r\nREASON");
	if (len >= size) {
		goto overflow;
	}
	size -= len;
	str += len;

	for (cpu = 0U; cpu < phys_cpu_num; cpu++) {
		len = snprintf(str, size, "\t      CPU%hu\t        US", cpu);
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;
	}

	for (i = 0U; i < 64U; i++) {
		len = snprintf(str, size, "\r\n0x%x", i);
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;
		for (cpu = 0U; cpu < phys_cpu_num; cpu++) {
			len = snprintf(str, size, "\t%10lld\t%10lld", per_cpu(vmexit_cnt, cpu)[i],
				ticks_to_us(per_cpu(vmexit_time, cpu)[i]));
			if (len >= size) {
				goto overflow;
			}

			size -= len;
			str += len;
		}
	}
	snprintf(str, size, "\r\n");
	return;

overflow:
	printf("buffer size could not be enough! please check!\n");
}
#endif /* HV_DEBUG */
