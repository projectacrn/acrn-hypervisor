/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <ucode.h>

enum rw_mode {
	DISABLE = 0U,
	READ,
	WRITE,
	READ_WRITE
};

/*
 * List of intercepted MSRs.
 * If any MSRs appear in this array but not handled in any swith statements
 * in either rdmsr_vmexit_handler() or wrmsr_vmexit_handler(), a GP will
 * be thrown to the guest for any R/W accesses.
 */
#define NUM_EMULATED_MSR	58U
static const uint32_t emulated_msrs[NUM_EMULATED_MSR] = {
	/* Emulated MSRs */
	MSR_IA32_TSC_DEADLINE,
	MSR_IA32_BIOS_UPDT_TRIG,
	MSR_IA32_BIOS_SIGN_ID,
	MSR_IA32_TIME_STAMP_COUNTER,
	MSR_IA32_PAT,
	MSR_IA32_APIC_BASE,

	MSR_IA32_PERF_CTL,

	MSR_IA32_MTRR_CAP,
	MSR_IA32_MTRR_DEF_TYPE,
	MSR_IA32_MTRR_FIX64K_00000,
	MSR_IA32_MTRR_FIX16K_80000,
	MSR_IA32_MTRR_FIX16K_A0000,
	MSR_IA32_MTRR_FIX4K_C0000,
	MSR_IA32_MTRR_FIX4K_C8000,
	MSR_IA32_MTRR_FIX4K_D0000,
	MSR_IA32_MTRR_FIX4K_D8000,
	MSR_IA32_MTRR_FIX4K_E0000,
	MSR_IA32_MTRR_FIX4K_E8000,
	MSR_IA32_MTRR_FIX4K_F0000,
	MSR_IA32_MTRR_FIX4K_F8000,

	/* Following MSRs intercepted, and throw GP for any access */

	/* Variable MTRRs are not supported */
	MSR_IA32_MTRR_PHYSBASE_0,
	MSR_IA32_MTRR_PHYSMASK_0,
	MSR_IA32_MTRR_PHYSBASE_1,
	MSR_IA32_MTRR_PHYSMASK_1,
	MSR_IA32_MTRR_PHYSBASE_2,
	MSR_IA32_MTRR_PHYSMASK_2,
	MSR_IA32_MTRR_PHYSBASE_3,
	MSR_IA32_MTRR_PHYSMASK_3,
	MSR_IA32_MTRR_PHYSBASE_4,
	MSR_IA32_MTRR_PHYSMASK_4,
	MSR_IA32_MTRR_PHYSBASE_5,
	MSR_IA32_MTRR_PHYSMASK_5,
	MSR_IA32_MTRR_PHYSBASE_6,
	MSR_IA32_MTRR_PHYSMASK_6,
	MSR_IA32_MTRR_PHYSBASE_7,
	MSR_IA32_MTRR_PHYSMASK_7,
	MSR_IA32_MTRR_PHYSBASE_8,
	MSR_IA32_MTRR_PHYSMASK_8,
	MSR_IA32_MTRR_PHYSBASE_9,
	MSR_IA32_MTRR_PHYSMASK_9,

	/* No level 2 VMX: CPUID.01H.ECX[5] */
	MSR_IA32_VMX_BASIC,
	MSR_IA32_VMX_PINBASED_CTLS,
	MSR_IA32_VMX_PROCBASED_CTLS,
	MSR_IA32_VMX_EXIT_CTLS,
	MSR_IA32_VMX_ENTRY_CTLS,
	MSR_IA32_VMX_MISC,
	MSR_IA32_VMX_CR0_FIXED0,
	MSR_IA32_VMX_CR0_FIXED1,
	MSR_IA32_VMX_CR4_FIXED0,
	MSR_IA32_VMX_CR4_FIXED1,
	MSR_IA32_VMX_VMCS_ENUM,
	MSR_IA32_VMX_PROCBASED_CTLS2,
	MSR_IA32_VMX_EPT_VPID_CAP,
	MSR_IA32_VMX_TRUE_PINBASED_CTLS,
	MSR_IA32_VMX_TRUE_PROCBASED_CTLS,
	MSR_IA32_VMX_TRUE_EXIT_CTLS,
	MSR_IA32_VMX_TRUE_ENTRY_CTLS,
	MSR_IA32_VMX_VMFUNC,
};

static const uint32_t x2apic_msrs[] = {
	MSR_IA32_EXT_XAPICID,
	MSR_IA32_EXT_APIC_VERSION,
	MSR_IA32_EXT_APIC_TPR,
	MSR_IA32_EXT_APIC_PPR,
	MSR_IA32_EXT_APIC_EOI,
	MSR_IA32_EXT_APIC_LDR,
	MSR_IA32_EXT_APIC_SIVR,
	MSR_IA32_EXT_APIC_ISR0,
	MSR_IA32_EXT_APIC_ISR1,
	MSR_IA32_EXT_APIC_ISR2,
	MSR_IA32_EXT_APIC_ISR3,
	MSR_IA32_EXT_APIC_ISR4,
	MSR_IA32_EXT_APIC_ISR5,
	MSR_IA32_EXT_APIC_ISR6,
	MSR_IA32_EXT_APIC_ISR7,
	MSR_IA32_EXT_APIC_TMR0,
	MSR_IA32_EXT_APIC_TMR1,
	MSR_IA32_EXT_APIC_TMR2,
	MSR_IA32_EXT_APIC_TMR3,
	MSR_IA32_EXT_APIC_TMR4,
	MSR_IA32_EXT_APIC_TMR5,
	MSR_IA32_EXT_APIC_TMR6,
	MSR_IA32_EXT_APIC_TMR7,
	MSR_IA32_EXT_APIC_IRR0,
	MSR_IA32_EXT_APIC_IRR1,
	MSR_IA32_EXT_APIC_IRR2,
	MSR_IA32_EXT_APIC_IRR3,
	MSR_IA32_EXT_APIC_IRR4,
	MSR_IA32_EXT_APIC_IRR5,
	MSR_IA32_EXT_APIC_IRR6,
	MSR_IA32_EXT_APIC_IRR7,
	MSR_IA32_EXT_APIC_ESR,
	MSR_IA32_EXT_APIC_LVT_CMCI,
	MSR_IA32_EXT_APIC_ICR,
	MSR_IA32_EXT_APIC_LVT_TIMER,
	MSR_IA32_EXT_APIC_LVT_THERMAL,
	MSR_IA32_EXT_APIC_LVT_PMI,
	MSR_IA32_EXT_APIC_LVT_LINT0,
	MSR_IA32_EXT_APIC_LVT_LINT1,
	MSR_IA32_EXT_APIC_LVT_ERROR,
	MSR_IA32_EXT_APIC_INIT_COUNT,
	MSR_IA32_EXT_APIC_CUR_COUNT,
	MSR_IA32_EXT_APIC_DIV_CONF,
	MSR_IA32_EXT_APIC_SELF_IPI,
};

static void enable_msr_interception(uint8_t *bitmap, uint32_t msr_arg, enum rw_mode mode)
{
	uint8_t *read_map;
	uint8_t *write_map;
	uint32_t msr = msr_arg;
	uint8_t msr_bit;
	uint32_t msr_index;
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
	msr_bit = 1U << (msr & 0x7U);
	msr_index = msr >> 3U;

	if ((mode & READ) == READ) {
		read_map[msr_index] |= msr_bit;
	} else {
		read_map[msr_index] &= ~msr_bit;
	}

	if ((mode & WRITE) == WRITE) {
		write_map[msr_index] |= msr_bit;
	} else {
		write_map[msr_index] &= ~msr_bit;
	}
}

/*
 * Enable read and write msr interception for x2APIC MSRs
 * MSRs that are not supported in the x2APIC range of MSRs,
 * i.e. anything other than the ones below and between
 * 0x802 and 0x83F, are not intercepted
 */

static void intercept_x2apic_msrs(uint8_t *msr_bitmap_arg, enum rw_mode mode)
{
	uint8_t *msr_bitmap = msr_bitmap_arg;
	uint32_t i;

	for (i = 0U; i < ARRAY_SIZE(x2apic_msrs); i++) {
		enable_msr_interception(msr_bitmap, x2apic_msrs[i], mode);
	}
}

static void init_msr_area(struct acrn_vcpu *vcpu)
{
	vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].msr_num = MSR_IA32_TSC_AUX;
	vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].value = vcpu->vcpu_id;
	vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].msr_num = MSR_IA32_TSC_AUX;
	vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].value = vcpu->pcpu_id;
}

void init_msr_emulation(struct acrn_vcpu *vcpu)
{
	uint32_t i;
	uint8_t *msr_bitmap;
	uint64_t value64;

	if (is_vcpu_bsp(vcpu)) {
		msr_bitmap = vcpu->vm->arch_vm.msr_bitmap;

		for (i = 0U; i < NUM_EMULATED_MSR; i++) {
			enable_msr_interception(msr_bitmap, emulated_msrs[i], READ_WRITE);
		}

		intercept_x2apic_msrs(msr_bitmap, READ_WRITE);
	}

	/* Setup MSR bitmap - Intel SDM Vol3 24.6.9 */
	value64 = hva2hpa(vcpu->vm->arch_vm.msr_bitmap);
	exec_vmwrite64(VMX_MSR_BITMAP_FULL, value64);
	pr_dbg("VMX_MSR_BITMAP: 0x%016llx ", value64);

	/* Initialize the MSR save/store area */
	init_msr_area(vcpu);
}

int rdmsr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int err = 0;
	uint32_t msr;
	uint64_t v = 0UL;

	/* Read the msr value */
	msr = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RCX);

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
		v = rdtsc() + exec_vmread64(VMX_TSC_OFFSET_FULL);
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
		err = -EACCES;
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
	case MSR_IA32_PAT:
	{
		v = vmx_rdmsr_pat(vcpu);
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
		if (is_x2apic_msr(msr)) {
			err = vlapic_rdmsr(vcpu, msr, &v);
		} else {
			pr_warn("%s(): vm%d vcpu%d reading MSR %lx not supported",
				__func__, vcpu->vm->vm_id, vcpu->vcpu_id, msr);
			err = -EACCES;
			v = 0UL;
		}
		break;
	}
	}

	/* Store the MSR contents in RAX and RDX */
	vcpu_set_gpreg(vcpu, CPU_REG_RAX, v & 0xffffffffU);
	vcpu_set_gpreg(vcpu, CPU_REG_RDX, v >> 32U);

	TRACE_2L(TRACE_VMEXIT_RDMSR, msr, v);

	return err;
}

int wrmsr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int err = 0;
	uint32_t msr;
	uint64_t v;

	/* Read the MSR ID */
	msr = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RCX);

	/* Get the MSR contents */
	v = (vcpu_get_gpreg(vcpu, CPU_REG_RDX) << 32U) |
		vcpu_get_gpreg(vcpu, CPU_REG_RAX);

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
		exec_vmwrite64(VMX_TSC_OFFSET_FULL, v - rdtsc());
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
		err = -EACCES;
#endif
		break;
	}
	case MSR_IA32_BIOS_SIGN_ID:
	{
		break;
	}
	case MSR_IA32_BIOS_UPDT_TRIG:
	{
		/* We only allow SOS to do uCode update */
		if (is_vm0(vcpu->vm)) {
			acrn_update_ucode(vcpu, v);
		}
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
	case MSR_IA32_PAT:
	{
		err = vmx_wrmsr_pat(vcpu, v);
		break;
	}
	case MSR_IA32_APIC_BASE:
	{
		err = vlapic_wrmsr(vcpu, msr, v);
		break;
	}
	default:
	{
		if (is_x2apic_msr(msr)) {
			err = vlapic_wrmsr(vcpu, msr, v);
		} else {
			pr_warn("%s(): vm%d vcpu%d writing MSR %lx not supported",
				__func__, vcpu->vm->vm_id, vcpu->vcpu_id, msr);
			err = -EACCES;
		}
		break;
	}
	}

	TRACE_2L(TRACE_VMEXIT_WRMSR, msr, v);

	return err;
}

void update_msr_bitmap_x2apic_apicv(struct acrn_vcpu *vcpu)
{
	uint8_t *msr_bitmap;

	msr_bitmap = vcpu->vm->arch_vm.msr_bitmap;
	intercept_x2apic_msrs(msr_bitmap, WRITE);
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_CUR_COUNT, READ);
	/*
	 * Open read-only interception for write-only
	 * registers to inject gp on reads. EOI and Self-IPI
	 * Writes are disabled for EOI, TPR and Self-IPI as
	 * writes to them are virtualized with Register Virtualization
	 * Refer to Section 29.1 in Intel SDM Vol. 3
	 */
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_TPR, DISABLE);
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_EOI, READ);
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_SELF_IPI, READ);
}

void update_msr_bitmap_x2apic_passthru(struct acrn_vcpu *vcpu)
{
	uint32_t msr;
	uint8_t *msr_bitmap;

	msr_bitmap = vcpu->vm->arch_vm.msr_bitmap;
	for (msr = MSR_IA32_EXT_XAPICID;
			msr <= MSR_IA32_EXT_APIC_SELF_IPI; msr++) {
		enable_msr_interception(msr_bitmap, msr, DISABLE);
	}
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_ICR, WRITE);
	enable_msr_interception(msr_bitmap, MSR_IA32_TSC_DEADLINE, DISABLE);
}
