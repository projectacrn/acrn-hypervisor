/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>

bool x2apic_enabled;

static void run_vcpu_pre_work(struct vcpu *vcpu)
{
	unsigned long *pending_pre_work = &vcpu->pending_pre_work;

	if (bitmap_test_and_clear(ACRN_VCPU_MMIO_COMPLETE, pending_pre_work))
		dm_emulate_mmio_post(vcpu);
}

void vcpu_thread(struct vcpu *vcpu)
{
	uint64_t vmexit_begin = 0, vmexit_end = 0;
	uint16_t basic_exit_reason = 0;
	uint64_t tsc_aux_hyp_cpu = vcpu->pcpu_id;
	int ret = 0;

	/* If vcpu is not launched, we need to do init_vmcs first */
	if (!vcpu->launched)
		init_vmcs(vcpu);

	run_vcpu_pre_work(vcpu);

	do {
		/* handling pending softirq */
		exec_softirq();

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

		vmexit_end = rdtsc();
		if (vmexit_begin > 0)
			per_cpu(vmexit_time, vcpu->pcpu_id)[basic_exit_reason]
				+= (vmexit_end - vmexit_begin);
		TRACE_2L(TRACE_VM_ENTER, 0, 0);

		/* Restore guest TSC_AUX */
		if (vcpu->launched) {
			CPU_MSR_WRITE(MSR_IA32_TSC_AUX,
					vcpu->msr_tsc_aux_guest);
		}

		ret = start_vcpu(vcpu);
		if (ret != 0) {
			pr_fatal("vcpu resume failed");
			pause_vcpu(vcpu, VCPU_ZOMBIE);
			continue;
		}

		vmexit_begin = rdtsc();

		vcpu->arch_vcpu.nrexits++;
		/* Save guest TSC_AUX */
		CPU_MSR_READ(MSR_IA32_TSC_AUX, &vcpu->msr_tsc_aux_guest);
		/* Restore native TSC_AUX */
		CPU_MSR_WRITE(MSR_IA32_TSC_AUX, tsc_aux_hyp_cpu);

		/* Dispatch handler */
		ret = vmexit_handler(vcpu);
		if (ret < 0) {
			pr_fatal("dispatch VM exit handler failed for reason"
				" %d, ret = %d!",
				vcpu->arch_vcpu.exit_reason & 0xFFFF, ret);
			vcpu_inject_gp(vcpu, 0);
			continue;
		}

		basic_exit_reason = vcpu->arch_vcpu.exit_reason & 0xFFFF;
		per_cpu(vmexit_cnt, vcpu->pcpu_id)[basic_exit_reason]++;
		TRACE_2L(TRACE_VM_EXIT, basic_exit_reason,
		vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].rip);
	} while (1);
}

static bool is_vm0_bsp(int pcpu_id)
{
	return pcpu_id == vm0_desc.vm_hw_logical_core_ids[0];
}

int hv_main(int cpu_id)
{
	int ret;

	pr_info("%s, Starting common entry point for CPU %d",
			__func__, cpu_id);

	if (cpu_id >= phy_cpu_num) {
		pr_err("%s, cpu_id %d out of range %d\n",
			__func__, cpu_id, phy_cpu_num);
		return -EINVAL;
	}

	if ((uint32_t) cpu_id != get_cpu_id()) {
		pr_err("%s, cpu_id %d mismatch\n", __func__, cpu_id);
		return -EINVAL;
	}

	/* Enable virtualization extensions */
	ret = exec_vmxon_instr(cpu_id);
	if (ret != 0)
		return ret;

	/* X2APIC mode is disabled by default. */
	x2apic_enabled = false;

	if (is_vm0_bsp(cpu_id)) {
		ret = prepare_vm0();
		if (ret != 0)
			return ret;
	}

	default_idle();

	return 0;
}

int get_vmexit_profile(char *str, int str_max)
{
	int cpu, i, len, size = str_max;

	len = snprintf(str, size, "\r\nNow(us) = %16lld\r\n",
			TICKS_TO_US(rdtsc()));
	size -= len;
	str += len;

	len = snprintf(str, size, "\r\nREASON");
	size -= len;
	str += len;

	for (cpu = 0; cpu < phy_cpu_num; cpu++) {
		len = snprintf(str, size, "\t      CPU%d\t        US", cpu);
		size -= len;
		str += len;
	}

	for (i = 0; i < 64; i++) {
		len = snprintf(str, size, "\r\n0x%x", i);
		size -= len;
		str += len;
		for (cpu = 0; cpu < phy_cpu_num; cpu++) {
			len = snprintf(str, size, "\t%10lld\t%10lld",
				per_cpu(vmexit_cnt, cpu)[i],
				TICKS_TO_US(per_cpu(vmexit_time, cpu)[i]));
			size -= len;
			str += len;
		}
	}
	snprintf(str, size, "\r\n");
	return 0;
}
