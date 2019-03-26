/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <pgtable.h>
#include <msr.h>
#include <vcpu.h>
#include <vm.h>
#include <vmcs.h>
#include <vmx.h>
#include <guest_pm.h>
#include <ucode.h>
#include <trace.h>
#include <logmsg.h>

#define INTERCEPT_DISABLE		(0U)
#define INTERCEPT_READ			(1U << 0U)
#define INTERCEPT_WRITE			(1U << 1U)
#define INTERCEPT_READ_WRITE		(INTERCEPT_READ | INTERCEPT_WRITE)

static const uint32_t emulated_guest_msrs[NUM_GUEST_MSRS] = {
	/*
	 * MSRs that trusty may touch and need isolation between secure and normal world
	 * This may include MSR_IA32_STAR, MSR_IA32_LSTAR, MSR_IA32_FMASK,
	 * MSR_IA32_KERNEL_GS_BASE, MSR_IA32_SYSENTER_ESP, MSR_IA32_SYSENTER_CS, MSR_IA32_SYSENTER_EIP
	 *
	 * Number of entries: NUM_WORLD_MSRS
	 */
	MSR_IA32_PAT,
	MSR_IA32_TSC_ADJUST,

	/*
	 * MSRs don't need isolation between worlds
	 * Number of entries: NUM_COMMON_MSRS
	 */
	MSR_IA32_TSC_DEADLINE,
	MSR_IA32_BIOS_UPDT_TRIG,
	MSR_IA32_BIOS_SIGN_ID,
	MSR_IA32_TIME_STAMP_COUNTER,
	MSR_IA32_APIC_BASE,
	MSR_IA32_PERF_CTL,
	MSR_IA32_FEATURE_CONTROL,
};

#define NUM_MTRR_MSRS	13U
static const uint32_t mtrr_msrs[NUM_MTRR_MSRS] = {
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
	MSR_IA32_MTRR_FIX4K_F8000
};

/* Following MSRs are intercepted, but it throws GPs for any guest accesses */
#define NUM_UNSUPPORTED_MSRS	104U
static const uint32_t unsupported_msrs[NUM_UNSUPPORTED_MSRS] = {
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
	MSR_IA32_SMRR_PHYSBASE,
	MSR_IA32_SMRR_PHYSMASK,

	/* No level 2 VMX: CPUID.01H.ECX[5] */
	MSR_IA32_SMBASE,
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

	/* SGX disabled: CPUID.12H.EAX[0], CPUID.07H.ECX[30] */
	MSR_IA32_SGXLEPUBKEYHASH0,
	MSR_IA32_SGXLEPUBKEYHASH1,
	MSR_IA32_SGXLEPUBKEYHASH2,
	MSR_IA32_SGXLEPUBKEYHASH3,

	/* SGX disabled : CPUID.07H.EBX[2] */
	MSR_IA32_SGX_SVN_STATUS,

	/* MPX disabled: CPUID.07H.EBX[14] */
	MSR_IA32_BNDCFGS,

	/* SGX disabled : CPUID.12H.EAX[0] */
	MSR_SGXOWNEREPOCH0,
	MSR_SGXOWNEREPOCH1,

	/* Performance Counters and Events: CPUID.0AH.EAX[15:8] */
	MSR_IA32_PMC0,
	MSR_IA32_PMC1,
	MSR_IA32_PMC2,
	MSR_IA32_PMC3,
	MSR_IA32_PMC4,
	MSR_IA32_PMC5,
	MSR_IA32_PMC6,
	MSR_IA32_PMC7,
	MSR_IA32_PERFEVTSEL0,
	MSR_IA32_PERFEVTSEL1,
	MSR_IA32_PERFEVTSEL2,
	MSR_IA32_PERFEVTSEL3,
	MSR_IA32_A_PMC0,
	MSR_IA32_A_PMC1,
	MSR_IA32_A_PMC2,
	MSR_IA32_A_PMC3,
	MSR_IA32_A_PMC4,
	MSR_IA32_A_PMC5,
	MSR_IA32_A_PMC6,
	MSR_IA32_A_PMC7,
	/* CPUID.0AH.EAX[7:0] */
	MSR_IA32_FIXED_CTR_CTL,
	MSR_IA32_PERF_GLOBAL_STATUS,
	MSR_IA32_PERF_GLOBAL_CTRL,
	MSR_IA32_PERF_GLOBAL_OVF_CTRL,
	MSR_IA32_PERF_GLOBAL_STATUS_SET,
	MSR_IA32_PERF_GLOBAL_INUSE,
	/* CPUID.0AH.EDX[4:0] */
	MSR_IA32_FIXED_CTR0,
	MSR_IA32_FIXED_CTR1,
	MSR_IA32_FIXED_CTR2,

	/* QOS Configuration disabled: CPUID.10H.ECX[2] */
	MSR_IA32_L3_QOS_CFG,
	MSR_IA32_L2_QOS_CFG,

	/* RDT-M disabled: CPUID.07H.EBX[12], CPUID.07H.EBX[15] */
	MSR_IA32_QM_EVTSEL,
	MSR_IA32_QM_CTR,
	MSR_IA32_PQR_ASSOC,

	/* RDT-A disabled: CPUID.07H.EBX[12], CPUID.10H */
	/* MSR 0xC90 ... 0xD8F, not in this array */

	/* RTIT disabled: CPUID.07H.EBX[25], CPUID.14H.ECX[0,2] (X86_FEATURE_INTEL_PT) */
	MSR_IA32_RTIT_OUTPUT_BASE,
	MSR_IA32_RTIT_OUTPUT_MASK_PTRS,
	MSR_IA32_RTIT_CTL,
	MSR_IA32_RTIT_STATUS,
	MSR_IA32_RTIT_CR3_MATCH,
	/* Region Address: CPUID.07H.EAX[2:0] (subleaf 1) */
	MSR_IA32_RTIT_ADDR0_A,
	MSR_IA32_RTIT_ADDR0_B,
	MSR_IA32_RTIT_ADDR1_A,
	MSR_IA32_RTIT_ADDR1_B,
	MSR_IA32_RTIT_ADDR2_A,
	MSR_IA32_RTIT_ADDR2_B,
	MSR_IA32_RTIT_ADDR3_A,
	MSR_IA32_RTIT_ADDR3_B,

	/* SMM Monitor Configuration: CPUID.01H.ECX[5] and CPUID.01H.ECX[6] */
	MSR_IA32_SMM_MONITOR_CTL,

	/* Silicon Debug Feature: CPUID.01H.ECX[11] (X86_FEATURE_SDBG) */
	MSR_IA32_DEBUG_INTERFACE,

	/* Performance Monitoring: CPUID.01H.ECX[15] X86_FEATURE_PDCM */
	MSR_IA32_PERF_CAPABILITIES,

	/* Debug Store disabled: CPUID.01H.EDX[21] X86_FEATURE_DTES */
	MSR_IA32_DS_AREA,

	/* Machine Check Exception: CPUID.01H.EDX[5] (X86_FEATURE_MCE) */
	MSR_IA32_MCG_CAP,
	MSR_IA32_MCG_STATUS,
	MSR_IA32_MCG_CTL,
	MSR_IA32_MCG_EXT_CTL,
	/* MSR 0x280 ... 0x29F, not in this array */
	/* MSR 0x400 ... 0x473, not in this array */
};

#define NUM_X2APIC_MSRS	44U
static const uint32_t x2apic_msrs[NUM_X2APIC_MSRS] = {
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

/* emulated_guest_msrs[] shares same indexes with array vcpu->arch->guest_msrs[] */
uint32_t vmsr_get_guest_msr_index(uint32_t msr)
{
	uint32_t index;

	for (index = 0U; index < NUM_GUEST_MSRS; index++) {
		if (emulated_guest_msrs[index] == msr) {
			break;
		}
	}

	if (index == NUM_GUEST_MSRS) {
		pr_err("%s, MSR %x is not defined in array emulated_guest_msrs[]", __func__, msr);
	}

	return index;
}

static void enable_msr_interception(uint8_t *bitmap, uint32_t msr_arg, uint32_t mode)
{
	uint32_t read_offset = 0U;
	uint32_t write_offset = 2048U;
	uint32_t msr = msr_arg;
	uint8_t msr_bit;
	uint32_t msr_index;

	if ((msr <= 0x1FFFU) || ((msr >= 0xc0000000U) && (msr <= 0xc0001fffU))) {
		if ((msr & 0xc0000000U) != 0U) {
			read_offset = read_offset + 1024U;
			write_offset = write_offset + 1024U;
		}

		msr &= 0x1FFFU;
		msr_bit = 1U << (msr & 0x7U);
		msr_index = msr >> 3U;

		if ((mode & INTERCEPT_READ) == INTERCEPT_READ) {
			bitmap[read_offset + msr_index] |= msr_bit;
		} else {
			bitmap[read_offset + msr_index] &= ~msr_bit;
		}

		if ((mode & INTERCEPT_WRITE) == INTERCEPT_WRITE) {
			bitmap[write_offset + msr_index] |= msr_bit;
		} else {
			bitmap[write_offset + msr_index] &= ~msr_bit;
		}
	} else {
		pr_err("%s, Invalid MSR: 0x%x", __func__, msr);
	}
}

/*
 * Enable read and write msr interception for x2APIC MSRs
 * MSRs that are not supported in the x2APIC range of MSRs,
 * i.e. anything other than the ones below and between
 * 0x802 and 0x83F, are not intercepted
 */

static void intercept_x2apic_msrs(uint8_t *msr_bitmap_arg, uint32_t mode)
{
	uint8_t *msr_bitmap = msr_bitmap_arg;
	uint32_t i;

	for (i = 0U; i < NUM_X2APIC_MSRS; i++) {
		enable_msr_interception(msr_bitmap, x2apic_msrs[i], mode);
	}
}

static void init_msr_area(struct acrn_vcpu *vcpu)
{
	vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].msr_index = MSR_IA32_TSC_AUX;
	vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].value = vcpu->vcpu_id;
	vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].msr_index = MSR_IA32_TSC_AUX;
	vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].value = vcpu->pcpu_id;
}

void init_msr_emulation(struct acrn_vcpu *vcpu)
{
	uint32_t msr, i;
	uint8_t *msr_bitmap;
	uint64_t value64;

	if (is_vcpu_bsp(vcpu)) {
		msr_bitmap = vcpu->vm->arch_vm.msr_bitmap;

		for (i = 0U; i < NUM_GUEST_MSRS; i++) {
			enable_msr_interception(msr_bitmap, emulated_guest_msrs[i], INTERCEPT_READ_WRITE);
		}

		for (i = 0U; i < NUM_MTRR_MSRS; i++) {
			enable_msr_interception(msr_bitmap, mtrr_msrs[i], INTERCEPT_READ_WRITE);
		}

		intercept_x2apic_msrs(msr_bitmap, INTERCEPT_READ_WRITE);

		for (i = 0U; i < NUM_UNSUPPORTED_MSRS; i++) {
			enable_msr_interception(msr_bitmap, unsupported_msrs[i], INTERCEPT_READ_WRITE);
		}

		/* RDT-A disabled: CPUID.07H.EBX[12], CPUID.10H */
		for (msr = MSR_IA32_L3_MASK_0; msr < MSR_IA32_BNDCFGS; msr++) {
			enable_msr_interception(msr_bitmap, msr, INTERCEPT_READ_WRITE);
		}

		/* don't need to intercept rdmsr for these MSRs */
		enable_msr_interception(msr_bitmap, MSR_IA32_TIME_STAMP_COUNTER, INTERCEPT_WRITE);
	}

	/* Setup MSR bitmap - Intel SDM Vol3 24.6.9 */
	value64 = hva2hpa(vcpu->vm->arch_vm.msr_bitmap);
	exec_vmwrite64(VMX_MSR_BITMAP_FULL, value64);
	pr_dbg("VMX_MSR_BITMAP: 0x%016llx ", value64);

	/* Initialize the MSR save/store area */
	init_msr_area(vcpu);
}

int32_t rdmsr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t err = 0;
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
	case MSR_IA32_TSC_ADJUST:
	{
		v = vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST);
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
		if (!vm_hide_mtrr(vcpu->vm)) {
			v = read_vmtrr(vcpu, msr);
		} else {
			err = -EACCES;
		}
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
	case MSR_IA32_FEATURE_CONTROL:
	{
		v = MSR_IA32_FEATURE_CONTROL_LOCK;
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

/*
 * Intel SDM 17.17.3: If an execution of WRMSR to the
 * IA32_TIME_STAMP_COUNTER MSR adds (or subtracts) value X from the
 * TSC, the logical processor also adds (or subtracts) value X from
 * the IA32_TSC_ADJUST MSR.
 */
static void set_guest_tsc(struct acrn_vcpu *vcpu, uint64_t guest_tsc)
{
	uint64_t tsc_delta, tsc_offset_delta, tsc_adjust;

	tsc_delta = guest_tsc - rdtsc();

	/* the delta between new and existing TSC_OFFSET */
	tsc_offset_delta = tsc_delta - exec_vmread64(VMX_TSC_OFFSET_FULL);

	/* apply this delta to TSC_ADJUST */
	tsc_adjust = vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST);
	vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_ADJUST, tsc_adjust + tsc_offset_delta);

	/* write to VMCS because rdtsc and rdtscp are not intercepted */
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, tsc_delta);
}

/*
 * Intel SDM 17.17.3: "If an execution of WRMSR to the IA32_TSC_ADJUST
 * MSR adds (or subtracts) value X from that MSR, the logical
 * processor also adds (or subtracts) value X from the TSC."
 */
static void set_guest_tsc_adjust(struct acrn_vcpu *vcpu, uint64_t tsc_adjust)
{
	uint64_t tsc_offset, tsc_adjust_delta;

	/* delta of the new and existing IA32_TSC_ADJUST */
	tsc_adjust_delta = tsc_adjust - vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST);

	/* apply this delta to existing TSC_OFFSET */
	tsc_offset = exec_vmread64(VMX_TSC_OFFSET_FULL);
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, tsc_offset + tsc_adjust_delta);

	/* IA32_TSC_ADJUST is supposed to carry the value it's written to */
	vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_ADJUST, tsc_adjust);
}

int32_t wrmsr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t err = 0;
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
	case MSR_IA32_TSC_ADJUST:
	{
		set_guest_tsc_adjust(vcpu, v);
		break;
	}
	case MSR_IA32_TIME_STAMP_COUNTER:
	{
		set_guest_tsc(vcpu, v);
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
		if (!vm_hide_mtrr(vcpu->vm)) {
			write_vmtrr(vcpu, msr, v);
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_BIOS_SIGN_ID:
	{
		break;
	}
	case MSR_IA32_BIOS_UPDT_TRIG:
	{
		/* We only allow SOS to do uCode update */
		if (is_sos_vm(vcpu->vm)) {
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
	case MSR_IA32_FEATURE_CONTROL:
	{
		err = -EACCES;
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

void update_msr_bitmap_x2apic_apicv(const struct acrn_vcpu *vcpu)
{
	uint8_t *msr_bitmap;

	msr_bitmap = vcpu->vm->arch_vm.msr_bitmap;
	/*
	 * For platforms that do not support register virtualization
	 * all x2APIC MSRs need to intercepted. So no need to update
	 * the MSR bitmap.
	 *
	 * TPR is virtualized even when register virtualization is not
	 * supported
	 */
	if (is_apicv_advanced_feature_supported()) {
		intercept_x2apic_msrs(msr_bitmap, INTERCEPT_WRITE);
		enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_CUR_COUNT, INTERCEPT_READ);
		/*
		 * Open read-only interception for write-only
		 * registers to inject gp on reads. EOI and Self-IPI
		 * Writes are disabled for EOI, TPR and Self-IPI as
		 * writes to them are virtualized with Register Virtualization
		 * Refer to Section 29.1 in Intel SDM Vol. 3
		 */
		enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_EOI, INTERCEPT_READ);
		enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_SELF_IPI, INTERCEPT_READ);
	}

	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_TPR, INTERCEPT_DISABLE);
}

/*
 * After switch to x2apic mode, most MSRs are passthrough to guest, but vlapic is still valid
 * for virtualization of some MSRs for security consideration:
 * - XAPICID/LDR: Read to XAPICID/LDR need to be trapped to guarantee guest always see right vlapic_id.
 * - ICR: Write to ICR need to be trapped to avoid milicious IPI.
 */
void update_msr_bitmap_x2apic_passthru(const struct acrn_vcpu *vcpu)
{
	uint32_t msr;
	uint8_t *msr_bitmap;

	msr_bitmap = vcpu->vm->arch_vm.msr_bitmap;
	for (msr = MSR_IA32_EXT_XAPICID;
			msr <= MSR_IA32_EXT_APIC_SELF_IPI; msr++) {
		enable_msr_interception(msr_bitmap, msr, INTERCEPT_DISABLE);
	}
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_XAPICID, INTERCEPT_READ);
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_LDR, INTERCEPT_READ);
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_ICR, INTERCEPT_WRITE);
	enable_msr_interception(msr_bitmap, MSR_IA32_TSC_DEADLINE, INTERCEPT_DISABLE);
	enable_msr_interception(msr_bitmap, MSR_IA32_TSC_ADJUST, INTERCEPT_DISABLE);
}
