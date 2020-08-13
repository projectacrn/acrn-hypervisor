/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <pgtable.h>
#include <msr.h>
#include <cpuid.h>
#include <vcpu.h>
#include <vm.h>
#include <vmx.h>
#include <sgx.h>
#include <guest_pm.h>
#include <ucode.h>
#include <rdt.h>
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

	MSR_IA32_MCG_CAP,
	MSR_IA32_MCG_STATUS,
	MSR_IA32_MISC_ENABLE,

	/* Don't support SGX Launch Control yet, read only */
	MSR_IA32_SGXLEPUBKEYHASH0,
	MSR_IA32_SGXLEPUBKEYHASH1,
	MSR_IA32_SGXLEPUBKEYHASH2,
	MSR_IA32_SGXLEPUBKEYHASH3,
	/* Read only */
	MSR_IA32_SGX_SVN_STATUS,

	MSR_IA32_XSS,

	MSR_TEST_CTL,
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
#define NUM_UNSUPPORTED_MSRS	111U
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

	/* PRMRR related MSRs are configured by native BIOS / bootloader */
	MSR_PRMRR_PHYS_BASE,
	MSR_PRMRR_PHYS_MASK,
	MSR_PRMRR_VALID_CONFIG,
	MSR_UNCORE_PRMRR_PHYS_BASE,
	MSR_UNCORE_PRMRR_PHYS_MASK,

	/*
	 * CET disabled:
	 * 	CPUID.07H.ECX[7] (CPUID_ECX_CET_SS)
	 * 	CPUID.07H.EDX[20] (CPUID_ECX_CET_IBT)
	 */
	MSR_IA32_U_CET,
	MSR_IA32_S_CET,
	MSR_IA32_PL0_SSP,
	MSR_IA32_PL1_SSP,
	MSR_IA32_PL2_SSP,
	MSR_IA32_PL3_SSP,
	MSR_IA32_INTERRUPT_SSP_TABLE_ADDR,
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
 */
static void intercept_x2apic_msrs(uint8_t *msr_bitmap_arg, uint32_t mode)
{
	uint8_t *msr_bitmap = msr_bitmap_arg;
	uint32_t msr;

	for (msr = 0x800U; msr < 0x900U; msr++) {
		enable_msr_interception(msr_bitmap, msr, mode);
	}
}

/**
 * @pre vcpu != NULL
 */
static void init_msr_area(struct acrn_vcpu *vcpu)
{
	struct acrn_vm_config *cfg = get_vm_config(vcpu->vm->vm_id);
	uint16_t vcpu_clos = cfg->clos[vcpu->vcpu_id];

	vcpu->arch.msr_area.count = 0U;

	vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].msr_index = MSR_IA32_TSC_AUX;
	vcpu->arch.msr_area.guest[MSR_AREA_TSC_AUX].value = vcpu->vcpu_id;
	vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].msr_index = MSR_IA32_TSC_AUX;
	vcpu->arch.msr_area.host[MSR_AREA_TSC_AUX].value = pcpuid_from_vcpu(vcpu);
	vcpu->arch.msr_area.count++;

	/* only load/restore MSR IA32_PQR_ASSOC when hv and guest have differnt settings */
	if (is_platform_rdt_capable() && (vcpu_clos != hv_clos)) {
		vcpu->arch.msr_area.guest[MSR_AREA_IA32_PQR_ASSOC].msr_index = MSR_IA32_PQR_ASSOC;
		vcpu->arch.msr_area.guest[MSR_AREA_IA32_PQR_ASSOC].value = clos2pqr_msr(vcpu_clos);
		vcpu->arch.msr_area.host[MSR_AREA_IA32_PQR_ASSOC].msr_index = MSR_IA32_PQR_ASSOC;
		vcpu->arch.msr_area.host[MSR_AREA_IA32_PQR_ASSOC].value = clos2pqr_msr(hv_clos);
		vcpu->arch.msr_area.count++;
		pr_acrnlog("switch clos for VM %u vcpu_id %u, host 0x%x, guest 0x%x",
			vcpu->vm->vm_id, vcpu->vcpu_id, hv_clos, vcpu_clos);
	}
}

/**
 * @pre vcpu != NULL
 */
void init_msr_emulation(struct acrn_vcpu *vcpu)
{
	uint8_t *msr_bitmap = vcpu->arch.msr_bitmap;
	uint32_t msr, i;
	uint64_t value64;

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
	for (msr = MSR_IA32_L3_MASK_BASE; msr < MSR_IA32_BNDCFGS; msr++) {
		enable_msr_interception(msr_bitmap, msr, INTERCEPT_READ_WRITE);
	}

	/* don't need to intercept rdmsr for these MSRs */
	enable_msr_interception(msr_bitmap, MSR_IA32_TIME_STAMP_COUNTER, INTERCEPT_WRITE);
	enable_msr_interception(msr_bitmap, MSR_IA32_XSS, INTERCEPT_WRITE);

	/* Setup MSR bitmap - Intel SDM Vol3 24.6.9 */
	value64 = hva2hpa(vcpu->arch.msr_bitmap);
	exec_vmwrite64(VMX_MSR_BITMAP_FULL, value64);
	pr_dbg("VMX_MSR_BITMAP: 0x%016lx ", value64);

	/* Initialize the MSR save/store area */
	init_msr_area(vcpu);
}

static int32_t write_pat_msr(struct acrn_vcpu *vcpu, uint64_t value)
{
	uint32_t i;
	uint64_t field;
	int32_t ret = 0;

	for (i = 0U; i < 8U; i++) {
		field = (value >> (i * 8U)) & 0xffUL;
		if (is_pat_mem_type_invalid(field)) {
			pr_err("invalid guest IA32_PAT: 0x%016lx", value);
			ret = -EINVAL;
			break;
		}
	}

	if (ret == 0) {
		vcpu_set_guest_msr(vcpu, MSR_IA32_PAT, value);

		/*
		 * If context->cr0.CD is set, we defer any further requests to write
		 * guest's IA32_PAT, until the time when guest's CR0.CD is being cleared
		 */
		if ((vcpu_get_cr0(vcpu) & CR0_CD) == 0UL) {
			exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, value);
		}
	}

	return ret;
}

/**
 * @pre vcpu != NULL
 */
int32_t rdmsr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t err = 0;
	uint32_t msr;
	uint64_t v = 0UL;

	/* Read the msr value */
	msr = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RCX);

	/* Do the required processing for each msr case */
	switch (msr) {
#ifdef CONFIG_HYPERV_ENABLED
	case HV_X64_MSR_GUEST_OS_ID:
	case HV_X64_MSR_HYPERCALL:
	case HV_X64_MSR_VP_INDEX:
	case HV_X64_MSR_REFERENCE_TSC:
	case HV_X64_MSR_TIME_REF_COUNT:
	{
		err = hyperv_rdmsr(vcpu, msr, &v);
		break;
	}
#endif
	case MSR_IA32_TSC_DEADLINE:
	{
		v = vlapic_get_tsc_deadline_msr(vcpu_vlapic(vcpu));
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
		/*
		 * note: if run_ctx->cr0.CD is set, the actual value in guest's
		 * IA32_PAT MSR is PAT_ALL_UC_VALUE, which may be different from
		 * the saved value guest_msrs[MSR_IA32_PAT]
		 */
		v = vcpu_get_guest_msr(vcpu, MSR_IA32_PAT);
		break;
	}
	case MSR_IA32_APIC_BASE:
	{
		/* Read APIC base */
		v = vlapic_get_apicbase(vcpu_vlapic(vcpu));
		break;
	}
	case MSR_IA32_FEATURE_CONTROL:
	{
		v = MSR_IA32_FEATURE_CONTROL_LOCK;
		if (is_vsgx_supported(vcpu->vm->vm_id)) {
			v |= MSR_IA32_FEATURE_CONTROL_SGX_GE;
		}
		break;
	}
	case MSR_IA32_MCG_CAP:
	case MSR_IA32_MCG_STATUS:
	{
		v = 0U;
		break;
	}
	case MSR_IA32_MISC_ENABLE:
	{
		v = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);
		break;
	}
	case MSR_IA32_SGXLEPUBKEYHASH0:
	case MSR_IA32_SGXLEPUBKEYHASH1:
	case MSR_IA32_SGXLEPUBKEYHASH2:
	case MSR_IA32_SGXLEPUBKEYHASH3:
	case MSR_IA32_SGX_SVN_STATUS:
	{
		if (is_vsgx_supported(vcpu->vm->vm_id)) {
			v = msr_read(msr);
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_TEST_CTL:
	{
		/* If has MSR_TEST_CTL, give emulated value
		 * If don't have MSR_TEST_CTL, trigger #GP
		 */
		if (has_core_cap(1U << 5U)) {
			v = vcpu_get_guest_msr(vcpu, MSR_TEST_CTL);
		} else {
			vcpu_inject_gp(vcpu, 0U);
		}
		break;
	}
	default:
	{
		if (is_x2apic_msr(msr)) {
			err = vlapic_x2apic_read(vcpu, msr, &v);
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
 * If VMX_TSC_OFFSET_FULL is 0, no need to trap the write of IA32_TSC_DEADLINE because there is
 * no offset between vTSC and pTSC, in this case, only write to vTSC_ADJUST is trapped.
 */
static void set_tsc_msr_interception(struct acrn_vcpu *vcpu, bool interception)
{
	uint8_t *msr_bitmap = vcpu->arch.msr_bitmap;
	bool is_intercepted =
		((msr_bitmap[MSR_IA32_TSC_DEADLINE >> 3U] & (1U << (MSR_IA32_TSC_DEADLINE & 0x7U))) != 0U);

	if (!interception && is_intercepted) {
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_DEADLINE, INTERCEPT_DISABLE);
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_ADJUST, INTERCEPT_WRITE);
		/* If the timer hasn't expired, sync virtual TSC_DEADLINE to physical TSC_DEADLINE, to make the guest read the same tsc_deadline
		 * as it writes. This may change when the timer actually trigger.
		 * If the timer has expired, write 0 to the virtual TSC_DEADLINE.
		 */
		if (msr_read(MSR_IA32_TSC_DEADLINE) != 0UL) {
			msr_write(MSR_IA32_TSC_DEADLINE, vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE));
		} else {
			vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE, 0UL);
		}
	} else if (interception && !is_intercepted) {
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_DEADLINE, INTERCEPT_READ_WRITE);
		enable_msr_interception(msr_bitmap, MSR_IA32_TSC_ADJUST, INTERCEPT_READ_WRITE);
		/* sync physical TSC_DEADLINE to virtual TSC_DEADLINE */
		vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE, msr_read(MSR_IA32_TSC_DEADLINE));
	} else {
		/* Do nothing */
	}
}

/*
 * Intel SDM 17.17.3: If an execution of WRMSR to the
 * IA32_TIME_STAMP_COUNTER MSR adds (or subtracts) value X from the
 * TSC, the logical processor also adds (or subtracts) value X from
 * the IA32_TSC_ADJUST MSR.
 *
 * So, here we should update VMCS.OFFSET and vAdjust accordingly.
 *   - VMCS.OFFSET = vTSC - pTSC
 *   - vAdjust += VMCS.OFFSET's delta
 */

/**
 * @pre vcpu != NULL
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

	set_tsc_msr_interception(vcpu, tsc_delta != 0UL);
}

/*
 * The policy of vART is that software in native can run in VM too. And in native side,
 * the relationship between the ART hardware and TSC is:
 *
 *   pTSC = (pART * M) / N + pAdjust
 *
 * The vART solution is:
 *   - Present the ART capability to guest through CPUID leaf
 *     15H for M/N which identical to the physical values.
 *   - PT devices see the pART (vART = pART).
 *   - Guest expect: vTSC = vART * M / N + vAdjust.
 *   - VMCS.OFFSET = vTSC - pTSC = vAdjust - pAdjust.
 *
 * So to support vART, we should do the following:
 *   1. if vAdjust and vTSC are changed by guest, we should change
 *      VMCS.OFFSET accordingly.
 *   2. Make the assumption that the pAjust is never touched by ACRN.
 */

/*
 * Intel SDM 17.17.3: "If an execution of WRMSR to the IA32_TSC_ADJUST
 * MSR adds (or subtracts) value X from that MSR, the logical
 * processor also adds (or subtracts) value X from the TSC."
 *
 * So, here we should update VMCS.OFFSET and vAdjust accordingly.
 *   - VMCS.OFFSET += vAdjust's delta
 *   - vAdjust = new vAdjust set by guest
 */

/**
 * @pre vcpu != NULL
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

	set_tsc_msr_interception(vcpu, (tsc_offset + tsc_adjust_delta ) != 0UL);
}

/**
 * @pre vcpu != NULL
 */
static void set_guest_ia32_misc_enalbe(struct acrn_vcpu *vcpu, uint64_t v)
{
	uint32_t eax, ebx = 0U, ecx = 0U, edx = 0U;
	bool update_vmsr = true;
	uint64_t msr_value;
	/* According to SDM Vol4 2.1 & Vol 3A 4.1.4,
	 * EFER.NXE should be cleared if guest disable XD in IA32_MISC_ENABLE
	 */
	if ((v & MSR_IA32_MISC_ENABLE_XD_DISABLE) != 0UL) {
		vcpu_set_efer(vcpu, vcpu_get_efer(vcpu) & ~MSR_IA32_EFER_NXE_BIT);
	}

	/* Handle MISC_ENABLE_MONITOR_ENA
	 * If has_monitor_cap() retrn true, this means the feature is enabed on host.
	 * HV will use monitor/mwait.
	 * - if guest try to set this bit, do nothing since it is already enabled
	 * - if guest try to clear this bit, not allow to disable in physcial MSR,
	 *   just clear the corresponding bit in vcpuid.
	 * If has_monitor_cap() retrn false, this means the feature is not enabled on host.
	 * HV will not use monitor/mwait. Allow guest to change the bit to physcial MSR
	 */
	if (((v ^ vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE)) & MSR_IA32_MISC_ENABLE_MONITOR_ENA) != 0UL) {
		eax = 1U;
		guest_cpuid(vcpu, &eax, &ebx, &ecx, &edx);
		/* According to SDM Vol4 2.1 Table 2-2,
		 * Writing this bit when the SSE3 feature flag is set to 0 may generate a #GP exception.
		 */
		if ((ecx & CPUID_ECX_SSE3) == 0U) {
			vcpu_inject_gp(vcpu, 0U);
			update_vmsr = false;
		} else if ((!has_monitor_cap()) && (!is_apl_platform())) {
			msr_value = msr_read(MSR_IA32_MISC_ENABLE) & ~MSR_IA32_MISC_ENABLE_MONITOR_ENA;
			msr_value |= v & MSR_IA32_MISC_ENABLE_MONITOR_ENA;
			/* This will not change the return value of has_monitor_cap() since the feature values
			 * are cached when platform init.
			 */
			msr_write(MSR_IA32_MISC_ENABLE, msr_value);
		} else {
			/* Not allow to change MISC_ENABLE_MONITOR_ENA in MSR */
		}
	}

	if (update_vmsr) {
		vcpu_set_guest_msr(vcpu, MSR_IA32_MISC_ENABLE, v);
	}
}

/**
 * @pre vcpu != NULL
 */
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
#ifdef CONFIG_HYPERV_ENABLED
	case HV_X64_MSR_GUEST_OS_ID:
	case HV_X64_MSR_HYPERCALL:
	case HV_X64_MSR_VP_INDEX:
	case HV_X64_MSR_REFERENCE_TSC:
	case HV_X64_MSR_TIME_REF_COUNT:
	{
		err = hyperv_wrmsr(vcpu, msr, v);
		break;
	}
#endif
	case MSR_IA32_TSC_DEADLINE:
	{
		vlapic_set_tsc_deadline_msr(vcpu_vlapic(vcpu), v);
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
		err = write_pat_msr(vcpu, v);
		break;
	}
	case MSR_IA32_APIC_BASE:
	{
		err = vlapic_set_apicbase(vcpu_vlapic(vcpu), v);
		break;
	}
	case MSR_IA32_MCG_STATUS:
	{
		if (v != 0U) {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_MCG_CAP:
	case MSR_IA32_FEATURE_CONTROL:
	case MSR_IA32_SGXLEPUBKEYHASH0:
	case MSR_IA32_SGXLEPUBKEYHASH1:
	case MSR_IA32_SGXLEPUBKEYHASH2:
	case MSR_IA32_SGXLEPUBKEYHASH3:
	case MSR_IA32_SGX_SVN_STATUS:
	{
		err = -EACCES;
		break;
	}
	case MSR_IA32_MISC_ENABLE:
	{
		set_guest_ia32_misc_enalbe(vcpu, v);
		break;
	}
	case MSR_IA32_XSS:
	{
		if ((v & ~(MSR_IA32_XSS_PT | MSR_IA32_XSS_HDC)) != 0UL) {
			err = -EACCES;
		} else {
			vcpu_set_guest_msr(vcpu, MSR_IA32_XSS, v);
			msr_write(msr, v);
		}
		break;
	}
	case MSR_TEST_CTL:
	{
		/* If VM has MSR_TEST_CTL, ignore write operation
		 * If don't have MSR_TEST_CTL, trigger #GP
		 */
		if (has_core_cap(1U << 5U)) {
			vcpu_set_guest_msr(vcpu, MSR_TEST_CTL, v);
			pr_warn("Ignore writting 0x%llx to MSR_TEST_CTL from VM%d", v, vcpu->vm->vm_id);
		} else {
			vcpu_inject_gp(vcpu, 0U);
		}
		break;
	}
	default:
	{
		if (is_x2apic_msr(msr)) {
			err = vlapic_x2apic_write(vcpu, msr, v);
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

/**
 * @pre vcpu != NULL
 */
void update_msr_bitmap_x2apic_apicv(struct acrn_vcpu *vcpu)
{
	uint8_t *msr_bitmap;

	msr_bitmap = vcpu->arch.msr_bitmap;
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
		enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_EOI, INTERCEPT_DISABLE);
		enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_SELF_IPI, INTERCEPT_DISABLE);
	}

	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_TPR, INTERCEPT_DISABLE);
}

/*
 * After switch to x2apic mode, most MSRs are passthrough to guest, but vlapic is still valid
 * for virtualization of some MSRs for security consideration:
 * - XAPICID/LDR: Read to XAPICID/LDR need to be trapped to guarantee guest always see right vlapic_id.
 * - ICR: Write to ICR need to be trapped to avoid milicious IPI.
 */

/**
 * @pre vcpu != NULL
 */
void update_msr_bitmap_x2apic_passthru(struct acrn_vcpu *vcpu)
{
	uint8_t *msr_bitmap = vcpu->arch.msr_bitmap;

	intercept_x2apic_msrs(msr_bitmap, INTERCEPT_DISABLE);
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_XAPICID, INTERCEPT_READ);
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_LDR, INTERCEPT_READ);
	enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_ICR, INTERCEPT_WRITE);
	set_tsc_msr_interception(vcpu, exec_vmread64(VMX_TSC_OFFSET_FULL) != 0UL);
}
