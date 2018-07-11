/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <ucode.h>

/*MRS need to be emulated, the order in this array better as freq of ops*/
static const uint32_t emulated_msrs[] = {
	MSR_IA32_TSC_DEADLINE,  /* Enable TSC_DEADLINE VMEXIT */
	MSR_IA32_BIOS_UPDT_TRIG, /* Enable MSR_IA32_BIOS_UPDT_TRIG */
	MSR_IA32_BIOS_SIGN_ID, /* Enable MSR_IA32_BIOS_SIGN_ID */
	MSR_IA32_TIME_STAMP_COUNTER,
	MSR_IA32_PAT,

/* following MSR not emulated now */
/*
 *	MSR_IA32_APIC_BASE,
 *	MSR_IA32_SYSENTER_CS,
 *	MSR_IA32_SYSENTER_ESP,
 *	MSR_IA32_SYSENTER_EIP,
 *	MSR_IA32_TSC_AUX,
 */
};

static void enable_msr_interception(uint8_t *bitmap, uint32_t msr)
{
	uint8_t *read_map;
	uint8_t *write_map;
	uint8_t value;
	/* low MSR */
	if (msr < 0x1FFFU) {
		read_map = bitmap;
		write_map = bitmap + 2048;
	} else if ((msr >= 0xc0000000U) && (msr <= 0xc0001fffU)) {
		read_map = bitmap + 1024;
		write_map = bitmap + 3072;
	} else {
		pr_err("Invalid MSR");
		return;
	}

	msr &= 0x1FFFU;
	value = read_map[(msr>>3)];
	value |= 1U<<(msr%8);
	/* right now we trap for both r/w */
	read_map[(msr>>3)] = value;
	write_map[(msr>>3)] = value;
}

/* not used now just leave it for some cases it may be used as API*/
void disable_msr_interception(uint8_t *bitmap, uint32_t msr)
{
	uint8_t *read_map;
	uint8_t *write_map;
	uint8_t value;
	/* low MSR */
	if (msr < 0x1FFFU) {
		read_map = bitmap;
		write_map = bitmap + 2048;
	} else if ((msr >= 0xc0000000U) && (msr <= 0xc0001fffU)) {
		read_map = bitmap + 1024;
		write_map = bitmap + 3072;
	} else {
		pr_err("Invalid MSR");
		return;
	}

	msr &= 0x1FFFU;
	value = read_map[(msr>>3)];
	value &= ~(1U<<(msr%8));
	/* right now we trap for both r/w */
	read_map[(msr>>3)] = value;
	write_map[(msr>>3)] = value;
}

void init_msr_emulation(struct vcpu *vcpu)
{
	uint32_t i = 0;
	uint32_t msrs_count =  ARRAY_SIZE(emulated_msrs);
	void *msr_bitmap;
	uint64_t value64;

	ASSERT(msrs_count == IDX_MAX_MSR,
		"MSR ID should be matched with emulated_msrs");

	/*msr bitmap, just allocated/init once, and used for all vm's vcpu*/
	if (is_vcpu_bsp(vcpu)) {

		/* Allocate and initialize memory for MSR bitmap region*/
		vcpu->vm->arch_vm.msr_bitmap = alloc_page();
		ASSERT(vcpu->vm->arch_vm.msr_bitmap != NULL, "");
		(void)memset(vcpu->vm->arch_vm.msr_bitmap, 0x0, CPU_PAGE_SIZE);

		msr_bitmap = vcpu->vm->arch_vm.msr_bitmap;

		for (i = 0U; i < msrs_count; i++) {
			enable_msr_interception(msr_bitmap, emulated_msrs[i]);
		}

		enable_msr_interception(msr_bitmap, MSR_IA32_PERF_CTL);

		/* below MSR protected from guest OS, if access to inject gp*/
		enable_msr_interception(msr_bitmap, MSR_IA32_MTRR_CAP);
		enable_msr_interception(msr_bitmap, MSR_IA32_MTRR_DEF_TYPE);

		for (i = MSR_IA32_MTRR_PHYSBASE_0;
			i <= MSR_IA32_MTRR_PHYSMASK_9; i++) {
			enable_msr_interception(msr_bitmap, i);
		}

		enable_msr_interception(msr_bitmap, MSR_IA32_MTRR_FIX64K_00000);
		enable_msr_interception(msr_bitmap, MSR_IA32_MTRR_FIX16K_80000);
		enable_msr_interception(msr_bitmap, MSR_IA32_MTRR_FIX16K_A0000);

		for (i = MSR_IA32_MTRR_FIX4K_C0000;
			i <= MSR_IA32_MTRR_FIX4K_F8000; i++) {
			enable_msr_interception(msr_bitmap, i);
		}

		for (i = MSR_IA32_VMX_BASIC;
			i <= MSR_IA32_VMX_TRUE_ENTRY_CTLS; i++) {
			enable_msr_interception(msr_bitmap, i);
		}
	}

	/* Set up MSR bitmap - pg 2904 24.6.9 */
	value64 = HVA2HPA(vcpu->vm->arch_vm.msr_bitmap);
	exec_vmwrite64(VMX_MSR_BITMAP_FULL, value64);
	pr_dbg("VMX_MSR_BITMAP: 0x%016llx ", value64);

	if (!vcpu->guest_msrs)
		vcpu->guest_msrs =
			(uint64_t *)calloc(msrs_count, sizeof(uint64_t));

	ASSERT(vcpu->guest_msrs != NULL, "");
	(void)memset(vcpu->guest_msrs, 0, msrs_count * sizeof(uint64_t));
}

int rdmsr_vmexit_handler(struct vcpu *vcpu)
{
	int err = 0;
	uint32_t msr;
	uint64_t v = 0UL;
	int cur_context = vcpu->arch_vcpu.cur_context;

	/* Read the msr value */
	msr = vcpu->arch_vcpu.contexts[cur_context].guest_cpu_regs.regs.rcx;

	/* Do the required processing for each msr case */
	switch (msr) {
	case MSR_IA32_TSC_DEADLINE:
	{
		err = vlapic_rdmsr(vcpu, msr, &v);
		break;
	}
	case MSR_IA32_TIME_STAMP_COUNTER:
	{
		/* Add the TSC_offset to host TSC to get guest TSC */
		v = rdtsc() + vcpu->arch_vcpu.contexts[cur_context].tsc_offset;
		break;
	}
	case MSR_IA32_MTRR_CAP:
	case MSR_IA32_MTRR_DEF_TYPE:
	case MSR_IA32_MTRR_FIX64K_00000:
	case MSR_IA32_MTRR_FIX16K_80000:
	case MSR_IA32_MTRR_FIX16K_A0000:
	case MSR_IA32_MTRR_FIX4K_C0000:
	case MSR_IA32_MTRR_FIX4K_C8000:
	case MSR_IA32_MTRR_FIX4K_D0000:
	case MSR_IA32_MTRR_FIX4K_D8000:
	case MSR_IA32_MTRR_FIX4K_E0000:
	case MSR_IA32_MTRR_FIX4K_E8000:
	case MSR_IA32_MTRR_FIX4K_F0000:
	case MSR_IA32_MTRR_FIX4K_F8000:
	{
#ifdef CONFIG_MTRR_ENABLED
		v = mtrr_rdmsr(vcpu, msr);
#else
		vcpu_inject_gp(vcpu, 0U);
#endif
		break;
	}
	case MSR_IA32_BIOS_SIGN_ID:
	{
		v = get_microcode_version();
		break;
	}
	case MSR_IA32_PERF_CTL:
	{
		v = msr_read(msr);
		break;
	}

	/* following MSR not emulated now just left for future */
	case MSR_IA32_SYSENTER_CS:
	{
		v = exec_vmread(VMX_GUEST_IA32_SYSENTER_CS);
		break;
	}
	case MSR_IA32_SYSENTER_ESP:
	{
		v = exec_vmread(VMX_GUEST_IA32_SYSENTER_ESP);
		break;
	}
	case MSR_IA32_SYSENTER_EIP:
	{
		v = exec_vmread(VMX_GUEST_IA32_SYSENTER_EIP);
		break;
	}
	case MSR_IA32_PAT:
	{
		v = vmx_rdmsr_pat(vcpu);
		break;
	}
	case MSR_IA32_TSC_AUX:
	{
		v = vcpu->arch_vcpu.msr_tsc_aux;
		break;
	}
	case MSR_IA32_APIC_BASE:
	{
		/* Read APIC base */
		err = vlapic_rdmsr(vcpu, msr, &v);
		break;
	}
	default:
	{
		if (!((msr >= MSR_IA32_MTRR_PHYSBASE_0 &&
			msr <= MSR_IA32_MTRR_PHYSMASK_9) ||
		      (msr >= MSR_IA32_VMX_BASIC &&
			msr <= MSR_IA32_VMX_TRUE_ENTRY_CTLS))) {
			pr_warn("rdmsr: %lx should not come here!", msr);
		}
		vcpu_inject_gp(vcpu, 0U);
		v = 0UL;
		break;
	}
	}

	/* Store the MSR contents in RAX and RDX */
	vcpu->arch_vcpu.contexts[cur_context].guest_cpu_regs.regs.rax =
					v & 0xffffffff;
	vcpu->arch_vcpu.contexts[cur_context].guest_cpu_regs.regs.rdx = v >> 32;

	TRACE_2L(TRACE_VMEXIT_RDMSR, msr, v);

	return err;
}

int wrmsr_vmexit_handler(struct vcpu *vcpu)
{
	int err = 0;
	uint32_t msr;
	uint64_t v;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	/* Read the MSR ID */
	msr = cur_context->guest_cpu_regs.regs.rcx;

	/* Get the MSR contents */
	v = (((uint64_t) cur_context->guest_cpu_regs.regs.rdx) << 32) |
	    ((uint64_t) cur_context->guest_cpu_regs.regs.rax);

	/* Do the required processing for each msr case */
	switch (msr) {
	case MSR_IA32_TSC_DEADLINE:
	{
		err = vlapic_wrmsr(vcpu, msr, v);
		break;
	}
	case MSR_IA32_TIME_STAMP_COUNTER:
	{
		/*Caculate TSC offset from changed TSC MSR value*/
		cur_context->tsc_offset = v - rdtsc();
		exec_vmwrite64(VMX_TSC_OFFSET_FULL, cur_context->tsc_offset);
		break;
	}

	case MSR_IA32_MTRR_DEF_TYPE:
	case MSR_IA32_MTRR_FIX64K_00000:
	case MSR_IA32_MTRR_FIX16K_80000:
	case MSR_IA32_MTRR_FIX16K_A0000:
	case MSR_IA32_MTRR_FIX4K_C0000:
	case MSR_IA32_MTRR_FIX4K_C8000:
	case MSR_IA32_MTRR_FIX4K_D0000:
	case MSR_IA32_MTRR_FIX4K_D8000:
	case MSR_IA32_MTRR_FIX4K_E0000:
	case MSR_IA32_MTRR_FIX4K_E8000:
	case MSR_IA32_MTRR_FIX4K_F0000:
	case MSR_IA32_MTRR_FIX4K_F8000:
	{
#ifdef CONFIG_MTRR_ENABLED
		mtrr_wrmsr(vcpu, msr, v);
#else
		vcpu_inject_gp(vcpu, 0U);
#endif
		break;
	}
	case MSR_IA32_MTRR_CAP:
	{
		vcpu_inject_gp(vcpu, 0U);
		break;
	}
	case MSR_IA32_BIOS_SIGN_ID:
	{
		break;
	}
	case MSR_IA32_BIOS_UPDT_TRIG:
	{
		/* We only allow SOS to do uCode update */
		if (is_vm0(vcpu->vm))
			acrn_update_ucode(vcpu, v);
		break;
	}
	case MSR_IA32_PERF_CTL:
	{
		if (validate_pstate(vcpu->vm, v) != 0) {
			break;
		}
		msr_write(msr, v);
		break;
	}

	/* following MSR not emulated now just left for future */
	case MSR_IA32_SYSENTER_CS:
	{
		exec_vmwrite(VMX_GUEST_IA32_SYSENTER_CS, v);
		break;
	}
	case MSR_IA32_SYSENTER_ESP:
	{
		exec_vmwrite(VMX_GUEST_IA32_SYSENTER_ESP, v);
		break;
	}
	case MSR_IA32_SYSENTER_EIP:
	{
		exec_vmwrite(VMX_GUEST_IA32_SYSENTER_EIP, v);
		break;
	}
	case MSR_IA32_PAT:
	{
		err = vmx_wrmsr_pat(vcpu, v);
		break;
	}
	case MSR_IA32_GS_BASE:
	{
		exec_vmwrite(VMX_GUEST_GS_BASE, v);
		break;
	}
	case MSR_IA32_TSC_AUX:
	{
		vcpu->arch_vcpu.msr_tsc_aux = v;
		break;
	}
	case MSR_IA32_APIC_BASE:
	{
		err = vlapic_wrmsr(vcpu, msr, v);
		break;
	}
	default:
	{
		if (!((msr >= MSR_IA32_MTRR_PHYSBASE_0 &&
			msr <= MSR_IA32_MTRR_PHYSMASK_9) ||
		      (msr >= MSR_IA32_VMX_BASIC &&
			msr <= MSR_IA32_VMX_TRUE_ENTRY_CTLS))) {
			pr_warn("rdmsr: %lx should not come here!", msr);
		}
		vcpu_inject_gp(vcpu, 0U);
		break;
	}
	}

	TRACE_2L(TRACE_VMEXIT_WRMSR, msr, v);

	return err;
}
