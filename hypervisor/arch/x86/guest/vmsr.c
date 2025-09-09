/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/pgtable.h>
#include <asm/msr.h>
#include <asm/cpuid.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/virq.h>
#include <asm/guest/vm.h>
#include <asm/vmx.h>
#include <asm/sgx.h>
#include <asm/guest/guest_pm.h>
#include <asm/guest/ucode.h>
#include <asm/guest/nested.h>
#include <asm/cpufeatures.h>
#include <asm/rdt.h>
#include <asm/tsc.h>
#include <trace.h>
#include <logmsg.h>
#include <asm/guest/vcat.h>
#include <per_cpu.h>

#define INTERCEPT_DISABLE		(0U)
#define INTERCEPT_READ			(1U << 0U)
#define INTERCEPT_WRITE			(1U << 1U)
#define INTERCEPT_READ_WRITE		(INTERCEPT_READ | INTERCEPT_WRITE)

static uint32_t emulated_guest_msrs[NUM_EMULATED_MSRS] = {
	/*
	 * MSRs that trusty may touch and need isolation between secure and normal world
	 * This may include MSR_IA32_STAR, MSR_IA32_LSTAR, MSR_IA32_FMASK,
	 * MSR_IA32_KERNEL_GS_BASE, MSR_IA32_SYSENTER_ESP, MSR_IA32_SYSENTER_CS, MSR_IA32_SYSENTER_EIP
	 *
	 * Number of entries: NUM_WORLD_MSRS
	 */
	MSR_IA32_PAT,
	MSR_IA32_EFER,
	MSR_IA32_TSC_ADJUST,

	/*
	 * MSRs don't need isolation between worlds
	 * Number of entries: NUM_COMMON_MSRS
	 */
	MSR_IA32_UMWAIT_CONTROL,
	MSR_IA32_TSC_DEADLINE,
	MSR_IA32_BIOS_UPDT_TRIG,
	MSR_IA32_BIOS_SIGN_ID,
	MSR_IA32_TIME_STAMP_COUNTER,
	MSR_IA32_APIC_BASE,
	MSR_IA32_PERF_STATUS,
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

	/* KeyLocker backup MSRs */
	MSR_IA32_COPY_LOCAL_TO_PLATFORM,
	MSR_IA32_COPY_PLATFORM_TO_LOCAL,
	MSR_IA32_COPY_STATUS,
	MSR_IA32_IWKEY_BACKUP_STATUS,

	MSR_TEST_CTL,

	MSR_PLATFORM_INFO,

	MSR_IA32_PM_ENABLE,
	MSR_IA32_HWP_CAPABILITIES,
	MSR_IA32_HWP_REQUEST,
	MSR_IA32_HWP_STATUS,
	MSR_IA32_MPERF,
	MSR_IA32_APERF,

	/*
	 * Thermal MSRs:
	 * CPUID.01H.EDX[22] IA32_THERM_INTERRUPT, IA32_THERM_STATUS, MSR_IA32_CLOCK_MODULATION
	 * CPUID.06H:EAX[6] IA32_PACKAGE_THERM_INTERRUPT, IA32_PACKAGE_THERM_STATUS
	 */
	MSR_IA32_CLOCK_MODULATION,
	MSR_IA32_THERM_INTERRUPT,
	MSR_IA32_THERM_STATUS,
	MSR_IA32_PACKAGE_THERM_INTERRUPT,
	MSR_IA32_PACKAGE_THERM_STATUS,

	/* VMX: CPUID.01H.ECX[5] */
#ifdef CONFIG_NVMX_ENABLED
	LIST_OF_VMX_MSRS,
#endif

	/* The following range of elements are reserved for vCAT usage and are
	 * initialized dynamically by init_intercepted_cat_msr_list() during platform initialization:
	 * [FLEXIBLE_MSR_INDEX ... (NUM_EMULATED_MSRS - 1)] = {
	 * The following layout of each CAT MSR entry is determined by cat_msr_to_index_of_emulated_msr():
	 * MSR_IA32_L3_MASK_BASE,
	 * MSR_IA32_L3_MASK_BASE + 1,
	 * ...
	 * MSR_IA32_L3_MASK_BASE + NUM_CAT_L3_MSRS - 1,
	 *
	 * MSR_IA32_L2_MASK_BASE + NUM_CAT_L3_MSRS,
	 * MSR_IA32_L2_MASK_BASE + NUM_CAT_L3_MSRS + 1,
	 * ...
	 * MSR_IA32_L2_MASK_BASE + NUM_CAT_L3_MSRS + NUM_CAT_L2_MSRS - 1,
	 *
	 * MSR_IA32_PQR_ASSOC + NUM_CAT_L3_MSRS + NUM_CAT_L2_MSRS
	 * }
	 */
};

static const uint32_t mtrr_msrs[] = {
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

/* Performance Counters and Events: CPUID.0AH.EAX[15:8] */
static const uint32_t pmc_msrs[] = {
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

	/* Performance Monitoring: CPUID.01H.ECX[15] X86_FEATURE_PDCM */
	MSR_IA32_PERF_CAPABILITIES,

	/* Debug Store disabled: CPUID.01H.EDX[21] X86_FEATURE_DTES */
	MSR_IA32_DS_AREA,
};

/* Following MSRs are intercepted, but it throws GPs for any guest accesses */
static const uint32_t unsupported_msrs[] = {
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

	/* VMX: CPUID.01H.ECX[5] */
#ifndef CONFIG_NVMX_ENABLED
	LIST_OF_VMX_MSRS,
#endif

	/* MPX disabled: CPUID.07H.EBX[14] */
	MSR_IA32_BNDCFGS,

	/* SGX disabled : CPUID.12H.EAX[0] */
	MSR_SGXOWNEREPOCH0,
	MSR_SGXOWNEREPOCH1,

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
	 * CPUID.07H.ECX[7] (CPUID_ECX_CET_SS)
	 * CPUID.07H.EDX[20] (CPUID_ECX_CET_IBT)
	 */
	MSR_IA32_U_CET,
	MSR_IA32_S_CET,
	MSR_IA32_PL0_SSP,
	MSR_IA32_PL1_SSP,
	MSR_IA32_PL2_SSP,
	MSR_IA32_PL3_SSP,
	MSR_IA32_INTERRUPT_SSP_TABLE_ADDR,

	/*
	 * HWP package ctrl disabled:
	 * CPUID.06H.EAX[11] (MSR_IA32_HWP_REQUEST_PKG)
	 * CPUID.06H.EAX[22] (MSR_IA32_HWP_CTL)
	 */
	MSR_IA32_HWP_REQUEST_PKG,
	MSR_IA32_HWP_CTL,

	/*
	 * HWP interrupt disabled:
	 * CPUID.06H.EAX[8]
	 */
	MSR_IA32_HWP_INTERRUPT,

	/*
	 * HFI and IDT registers disabled:
	 * CPUID.06H.EAX[19]
	 * CPUID.06H.EAX[23]
	 */
	IA32_HW_FEEDBACK_PTR,
	IA32_HW_FEEDBACK_CONFIG,
	IA32_THREAD_FEEDBACK_CHAR,
	IA32_HW_FEEDBACK_THREAD_CONFIG,
};

/* emulated_guest_msrs[] shares same indexes with array vcpu->arch->guest_msrs[] */
uint32_t vmsr_get_guest_msr_index(uint32_t msr)
{
	uint32_t index;

	for (index = 0U; index < NUM_EMULATED_MSRS; index++) {
		if (emulated_guest_msrs[index] == msr) {
			break;
		}
	}

	if (index == NUM_EMULATED_MSRS) {
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
		msr_bit = (uint8_t)(1U << (msr & 0x7U));
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
 * @pre vcpu != NULL && vcpu->vm != NULL && vcpu->vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (is_platform_rdt_capable() == false()) || (is_platform_rdt_capable() && get_vm_config(vcpu->vm->vm_id)->pclosids != NULL)
 */
static void prepare_auto_msr_area(struct acrn_vcpu *vcpu)
{
	vcpu->arch.msr_area.count = 0U;

	/* in HV, disable perf/PMC counting, just count in guest VM */
	if (is_pmu_pt_configured(vcpu->vm)) {
		vcpu->arch.msr_area.guest[vcpu->arch.msr_area.count].msr_index = MSR_IA32_PERF_GLOBAL_CTRL;
		vcpu->arch.msr_area.guest[vcpu->arch.msr_area.count].value = 0;
		vcpu->arch.msr_area.host[vcpu->arch.msr_area.count].msr_index = MSR_IA32_PERF_GLOBAL_CTRL;
		vcpu->arch.msr_area.host[vcpu->arch.msr_area.count].value = 0;
		vcpu->arch.msr_area.count++;
	}

	if (is_platform_rdt_capable()) {
		struct acrn_vm_config *cfg = get_vm_config(vcpu->vm->vm_id);
		uint16_t vcpu_clos;

		ASSERT(cfg->pclosids != NULL, "error, cfg->pclosids is NULL");

		vcpu_clos = cfg->pclosids[vcpu->vcpu_id%cfg->num_pclosids];

		/* RDT: only load/restore MSR_IA32_PQR_ASSOC when hv and guest have different settings
		 * vCAT: always load/restore MSR_IA32_PQR_ASSOC
		 */
		if (is_vcat_configured(vcpu->vm) || (vcpu_clos != hv_clos)) {
			vcpu->arch.msr_area.guest[vcpu->arch.msr_area.count].msr_index = MSR_IA32_PQR_ASSOC;
			vcpu->arch.msr_area.guest[vcpu->arch.msr_area.count].value = clos2pqr_msr(vcpu_clos);
			vcpu->arch.msr_area.host[vcpu->arch.msr_area.count].msr_index = MSR_IA32_PQR_ASSOC;
			vcpu->arch.msr_area.host[vcpu->arch.msr_area.count].value = clos2pqr_msr(hv_clos);
			vcpu->arch.msr_area.index_of_pqr_assoc = vcpu->arch.msr_area.count;
			vcpu->arch.msr_area.count++;

			pr_acrnlog("switch clos for VM %u vcpu_id %u, host 0x%x, guest 0x%x",
				vcpu->vm->vm_id, vcpu->vcpu_id, hv_clos, vcpu_clos);
		}
	}

	ASSERT(vcpu->arch.msr_area.count <= MSR_AREA_COUNT, "error, please check MSR_AREA_COUNT!");
}

/**
 * @pre vcpu != NULL
 */
void init_emulated_msrs(struct acrn_vcpu *vcpu)
{
	uint64_t val64 = 0UL;

	/* MSR_IA32_FEATURE_CONTROL */
	if (is_nvmx_configured(vcpu->vm)) {
		/* currently support VMX outside SMX only */
		val64 |= MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX;
	}

	val64 |= MSR_IA32_FEATURE_CONTROL_LOCK;
	if (is_vsgx_supported(vcpu->vm->vm_id)) {
		val64 |= MSR_IA32_FEATURE_CONTROL_SGX_GE;
	}

	vcpu_set_guest_msr(vcpu, MSR_IA32_FEATURE_CONTROL, val64);

#ifdef CONFIG_VCAT_ENABLED
	/*
	 * init_vcat_msrs() will overwrite the vcpu->arch.msr_area.guest[].value for MSR_IA32_PQR_ASSOC
	 * set by prepare_auto_msr_area()
	 */
	init_vcat_msrs(vcpu);
#endif
}

#ifdef CONFIG_VCAT_ENABLED
/**
 * @brief Map CAT MSR address to zero based index
 *
 * @pre  ((msr >= MSR_IA32_L3_MASK_BASE) && msr < (MSR_IA32_L3_MASK_BASE + NUM_CAT_L3_MSRS))
 *       || ((msr >= MSR_IA32_L2_MASK_BASE) && msr < (MSR_IA32_L2_MASK_BASE + NUM_CAT_L2_MSRS))
 *       || (msr == MSR_IA32_PQR_ASSOC)
 */
static uint32_t cat_msr_to_index_of_emulated_msr(uint32_t msr)
{
	uint32_t index = 0U;

	/*  L3 MSRs indices assignment for MSR_IA32_L3_MASK_BASE ~ (MSR_IA32_L3_MASK_BASE + NUM_CAT_L3_MSRS):
	 *  0
	 *  1
	 *  ...
	 *  (NUM_CAT_L3_MSRS - 1)
	 *
	 *  L2 MSRs indices assignment:
	 *  NUM_CAT_L3_MSRS
	 *  ...
	 *  NUM_CAT_L3_MSRS + NUM_CAT_L2_MSRS - 1

	 *  PQR index assignment for MSR_IA32_PQR_ASSOC:
	 *  NUM_CAT_L3_MSRS
	 */

	if ((msr >= MSR_IA32_L3_MASK_BASE) && (msr < (MSR_IA32_L3_MASK_BASE + NUM_CAT_L3_MSRS))) {
		index = msr - MSR_IA32_L3_MASK_BASE;
	} else if ((msr >= MSR_IA32_L2_MASK_BASE) && (msr < (MSR_IA32_L2_MASK_BASE + NUM_CAT_L2_MSRS))) {
		index = msr - MSR_IA32_L2_MASK_BASE + NUM_CAT_L3_MSRS;
	} else if (msr == MSR_IA32_PQR_ASSOC) {
		index = NUM_CAT_L3_MSRS + NUM_CAT_L2_MSRS;
	} else {
		ASSERT(false, "invalid CAT msr address");
	}

	return index;
}

static void init_cat_msr_entry(uint32_t msr)
{
	/* Get index into the emulated_guest_msrs[] table for a given CAT MSR.
	 * CAT MSR starts from FLEXIBLE_MSR_INDEX in the emulated MSR list.
	 */
	uint32_t index = cat_msr_to_index_of_emulated_msr(msr) + FLEXIBLE_MSR_INDEX;

	emulated_guest_msrs[index] = msr;
}

/* Init emulated_guest_msrs[] dynamically for CAT MSRs */
void init_intercepted_cat_msr_list(void)
{
	uint32_t msr;

	/* MSR_IA32_L2_MASK_n MSRs */
	for (msr = MSR_IA32_L2_MASK_BASE; msr < (MSR_IA32_L2_MASK_BASE + NUM_CAT_L2_MSRS); msr++) {
		init_cat_msr_entry(msr);
	}

	/* MSR_IA32_L3_MASK_n MSRs */
	for (msr = MSR_IA32_L3_MASK_BASE; msr < (MSR_IA32_L3_MASK_BASE + NUM_CAT_L3_MSRS); msr++) {
		init_cat_msr_entry(msr);
	}

	/* MSR_IA32_PQR_ASSOC */
	init_cat_msr_entry(MSR_IA32_PQR_ASSOC);
}
#endif

/**
 * @pre vcpu != NULL
 */
void init_msr_emulation(struct acrn_vcpu *vcpu)
{
	uint8_t *msr_bitmap = vcpu->arch.msr_bitmap;
	uint32_t msr, i;
	uint64_t value64;

	for (i = 0U; i < NUM_EMULATED_MSRS; i++) {
		enable_msr_interception(msr_bitmap, emulated_guest_msrs[i], INTERCEPT_READ_WRITE);
	}

	for (i = 0U; i < ARRAY_SIZE(mtrr_msrs); i++) {
		enable_msr_interception(msr_bitmap, mtrr_msrs[i], INTERCEPT_READ_WRITE);
	}

	/* for core partition VM (like RTVM), passthrou PMC MSRs for performance profiling/tuning; hide to other VMs */
	if (!is_pmu_pt_configured(vcpu->vm)) {
		for (i = 0U; i < ARRAY_SIZE(pmc_msrs); i++) {
			enable_msr_interception(msr_bitmap, pmc_msrs[i], INTERCEPT_READ_WRITE);
		}
	}

	intercept_x2apic_msrs(msr_bitmap, INTERCEPT_READ_WRITE);

	for (i = 0U; i < ARRAY_SIZE(unsupported_msrs); i++) {
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
	prepare_auto_msr_area(vcpu);

	/* Setup initial value for emulated MSRs */
	init_emulated_msrs(vcpu);

	/* Initialize VMX MSRs for nested virtualization */
	init_vmx_msrs(vcpu);
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

/*
 * @brief get emulated IA32_PERF_STATUS reg value
 *
 * Use the base frequency state of pCPU as the emulated reg field:
 *   - IA32_PERF_STATUS[15:0] Current performance State Value
 *
 * Assuming (base frequency ratio << 8) is a valid state value for all CPU models.
 */
static uint64_t get_perf_status(void)
{
	uint32_t eax, ecx, unused;
	/*
	 * CPUID.16H:eax[15:0] Base CPU Frequency (MHz)
	 * CPUID.16H:ecx[15:0] Bus Frequency (MHz)
	 * ratio = CPU_frequency/bus_frequency
	 */
	cpuid_subleaf(0x16U, 0U, &eax, &unused, &ecx, &unused);
	return (uint64_t)(((eax/ecx) & 0xFFU) << 8);
}

/**
 * @pre vcpu != NULL
 */
bool is_iwkey_backup_support(struct acrn_vcpu *vcpu)
{
	uint32_t eax = 0x19U, ebx = 0U, ecx = 0U, edx = 0U;

	guest_cpuid(vcpu, &eax, &ebx, &ecx, &edx);
	return (ebx & CPUID_EBX_KL_BACKUP_MSR) == CPUID_EBX_KL_BACKUP_MSR;
}

/**
 * @pre vcpu != NULL
 */
bool is_ecmd_supported(struct acrn_vcpu *vcpu)
{
	uint32_t eax = 0x6U, ebx = 0U, ecx = 0U, edx = 0U;
	/* ECMD. Check clock modulation duty cycle extension is supported */
	guest_cpuid(vcpu, &eax, &ebx, &ecx, &edx);
	return (eax & CPUID_EAX_ECMD) == CPUID_EAX_ECMD;
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
	case HV_X64_MSR_TSC_FREQUENCY:
	case HV_X64_MSR_APIC_FREQUENCY:
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
	case MSR_IA32_CLOCK_MODULATION:
	case MSR_IA32_THERM_STATUS:
	case MSR_IA32_THERM_INTERRUPT:
	case MSR_IA32_PACKAGE_THERM_INTERRUPT:
	case MSR_IA32_PACKAGE_THERM_STATUS:
	{
		v = msr_read(msr);
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
	case MSR_IA32_PERF_STATUS:
	{
		if (is_vhwp_configured(vcpu->vm)) {
			v = msr_read(msr);
		} else {
			v = get_perf_status();
		}
		break;
	}
	case MSR_IA32_PERF_CTL:
	{
		v = vcpu_get_guest_msr(vcpu, MSR_IA32_PERF_CTL);
		break;
	}
	case MSR_IA32_PM_ENABLE:
	case MSR_IA32_HWP_CAPABILITIES:
	case MSR_IA32_HWP_REQUEST:
	case MSR_IA32_HWP_STATUS:
	case MSR_IA32_MPERF:
	case MSR_IA32_APERF:
	{
		if (is_vhwp_configured(vcpu->vm)) {
			v = msr_read(msr);
		} else {
			err = -EACCES;
		}
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
	case MSR_IA32_EFER:
	{
		v = vcpu_get_efer(vcpu);
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
		v = vcpu_get_guest_msr(vcpu, MSR_IA32_FEATURE_CONTROL);
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
		/* As CPUID.01H:ECX[7] is removed from guests, guests should not see EIST enable bit. */
		v &= ~MSR_IA32_MISC_ENABLE_EIST;
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
	case MSR_IA32_COPY_STATUS:
	{
		if (is_iwkey_backup_support(vcpu)) {
			v = vcpu->arch.iwkey_copy_status;
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_IWKEY_BACKUP_STATUS:
	{
		if (is_iwkey_backup_support(vcpu)) {
			v = vcpu->vm->arch_vm.iwkey_backup_status;
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_UMWAIT_CONTROL:
	{
		/* Feature X86_FEATURE_WAITPKG is always presented */
		if (pcpu_has_cap(X86_FEATURE_WAITPKG)) {
			v = vcpu_get_guest_msr(vcpu, msr);
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
		if (has_core_cap(CORE_CAP_SPLIT_LOCK) || has_core_cap(CORE_CAP_UC_LOCK)) {
			v = vcpu_get_guest_msr(vcpu, MSR_TEST_CTL);
		} else {
			vcpu_inject_gp(vcpu, 0U);
		}
		break;
	}
	case MSR_PLATFORM_INFO:
	{
		if (is_service_vm(vcpu->vm)) {
			v = msr_read(msr);
			v &= MSR_PLATFORM_INFO_MAX_NON_TURBO_LIM_RATIO_MASK |
			     MSR_PLATFORM_INFO_MAX_EFFICIENCY_RATIO_MASK |
			     MSR_PLATFORM_INFO_MIN_OPERATING_RATIO_MASK |
			     MSR_PLATFORM_INFO_SAMPLE_PART;
		} else {
			/* Allow read by non-service vm for compatibility */
			v = 0UL;
			pr_warn("%s(): vm%d read MSR_PLATFORM_INFO", __func__, vcpu->vm->vm_id);
		}
		break;
	}
#ifdef CONFIG_VCAT_ENABLED
	case MSR_IA32_L2_MASK_BASE ... (MSR_IA32_L2_MASK_BASE +  NUM_CAT_L2_MSRS - 1U):
	case MSR_IA32_L3_MASK_BASE ... (MSR_IA32_L3_MASK_BASE +  NUM_CAT_L3_MSRS - 1U):
	{
		err = read_vcbm(vcpu, msr, &v);
		break;
	}
	case MSR_IA32_PQR_ASSOC:
	{
		err = read_vclosid(vcpu, &v);
		break;
	}
#endif
	default:
	{
		if (is_x2apic_msr(msr)) {
			err = vlapic_x2apic_read(vcpu, msr, &v);
		} else if (is_vmx_msr(msr)) {
			/*
			 * TODO: after the switch statement in this function, there is another
			 * switch statement inside read_vmx_msr(). Is it possible to reduce it
			 * to just one switch to improvement  performance?
			 */
			err = read_vmx_msr(vcpu, msr, &v);
		} else {
			pr_warn("%s(): vm%d vcpu%d reading MSR %lx not supported",
				__func__, vcpu->vm->vm_id, vcpu->vcpu_id, msr);
			err = -EACCES;
			v = 0UL;
		}
		break;
	}
	}

	if (err == 0) {
		/* Store the MSR contents in RAX and RDX */
		vcpu_set_gpreg(vcpu, CPU_REG_RAX, v & 0xffffffffU);
		vcpu_set_gpreg(vcpu, CPU_REG_RDX, v >> 32U);
	}

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

	set_tsc_msr_interception(vcpu, (tsc_offset + tsc_adjust_delta) != 0UL);
}

/**
 * @pre vcpu != NULL
 */
static void set_guest_ia32_misc_enalbe(struct acrn_vcpu *vcpu, uint64_t v)
{
	bool update_vmsr = true;
	uint64_t effective_guest_msr = v;

	/* According to SDM Vol4 2.1 & Vol 3A 4.1.4,
	 * EFER.NXE should be cleared if guest disable XD in IA32_MISC_ENABLE
	 */
	if ((v & MSR_IA32_MISC_ENABLE_XD_DISABLE) != 0UL) {
		vcpu_set_efer(vcpu, vcpu_get_efer(vcpu) & ~MSR_IA32_EFER_NXE_BIT);
	}

	/* MONITOR/MWAIT is hide.
	 * MISC_ENABLE_MONITOR_ENA should not be set.
	 */
	if ((v & MSR_IA32_MISC_ENABLE_MONITOR_ENA) != 0UL) {
		vcpu_inject_gp(vcpu, 0U);
		update_vmsr = false;
	}

	if (update_vmsr) {
		vcpu_set_guest_msr(vcpu, MSR_IA32_MISC_ENABLE, effective_guest_msr);
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
	case HV_X64_MSR_TSC_FREQUENCY:
	case HV_X64_MSR_APIC_FREQUENCY:
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
	case MSR_IA32_CLOCK_MODULATION:
	{
		if (is_vtm_configured(vcpu->vm)) {
			/*if extended clock modulation duty(ECMD) is not supported,
			 *bit 0 is reserved.
			 */
			if (is_ecmd_supported(vcpu)) {
				err = msr_write_safe(msr, v, MSR_IA32_CLOCK_MODULATION_RSV_BITS);
			} else {
				err = msr_write_safe(msr, v, MSR_IA32_CLOCK_MODULATION_RSV_BITS |
					0x1UL);
			}
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_THERM_STATUS:
	{
		if (is_vtm_configured(vcpu->vm)) {
			err = msr_write_safe(msr, v, MSR_IA32_THERM_STATUS_RSV_BITS);
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_THERM_INTERRUPT:
	{
		if (is_vtm_configured(vcpu->vm)) {
			err = msr_write_safe(msr, v, MSR_IA32_THERM_INTERRUPT_RSV_BITS);
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_PACKAGE_THERM_INTERRUPT:
	{
		if (is_vtm_configured(vcpu->vm)) {
			err = msr_write_safe(msr, v, MSR_IA32_PACKAGE_THERM_INTERRUPT_RSV_BITS);
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_PACKAGE_THERM_STATUS:
	{
		if (is_vtm_configured(vcpu->vm)) {
			err = msr_write_safe(msr, v, MSR_IA32_PACKAGE_THERM_STATUS_RSV_BITS);
		} else {
			err = -EACCES;
		}
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
		/* We only allow Service VM to do uCode update */
		if (is_service_vm(vcpu->vm)) {
			acrn_update_ucode(vcpu, v);
		}
		break;
	}
	case MSR_IA32_PERF_STATUS:
	{
		break;
	}
	case MSR_IA32_PERF_CTL:
	{
		vcpu_set_guest_msr(vcpu, MSR_IA32_PERF_CTL, v);
		break;
	}
	case MSR_IA32_PM_ENABLE:
	{
		if (!is_vhwp_configured(vcpu->vm)) {
			err = -EACCES;
		}
		/* Set by HV. Writing from guests will have no effect */
		break;
	}
	case MSR_IA32_HWP_CAPABILITIES:
	{
		/* RO */
		break;
	}
	case MSR_IA32_HWP_REQUEST:
	{
		if (is_vhwp_configured(vcpu->vm) &&
			((v & (MSR_IA32_HWP_REQUEST_RSV_BITS | MSR_IA32_HWP_REQUEST_PKG_CTL)) == 0)) {
			msr_write(msr, v);
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_HWP_STATUS:
	{
		if (is_vhwp_configured(vcpu->vm) && ((v & MSR_IA32_HWP_STATUS_RSV_BITS) == 0)) {
			msr_write(msr, v);
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_MPERF:
	case MSR_IA32_APERF:
	{
		if (is_vhwp_configured(vcpu->vm)) {
			msr_write(msr, v);
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_PAT:
	{
		err = write_pat_msr(vcpu, v);
		break;
	}
	case MSR_IA32_EFER:
	{
		vcpu_set_efer(vcpu, v);
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
		if (vcpu->arch.xsave_enabled) {
			if ((v & ~(MSR_IA32_XSS_PT | MSR_IA32_XSS_HDC)) != 0UL) {
				err = -EACCES;
			} else {
				vcpu_set_guest_msr(vcpu, MSR_IA32_XSS, v);
				msr_write(msr, v);
			}
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_IA32_COPY_LOCAL_TO_PLATFORM:
	{	
		/* check feature support and avoid setting reserved MSR bits */
		if (is_iwkey_backup_support(vcpu) && ((v & ~0x1UL) == 0x0UL)) {
			if (v == 0x1UL) {
				vcpu->vm->arch_vm.iwkey_backup_status = 0UL;
				spinlock_obtain(&vcpu->vm->arch_vm.iwkey_backup_lock);
				vcpu->vm->arch_vm.iwkey_backup = vcpu->arch.IWKey;
				spinlock_release(&vcpu->vm->arch_vm.iwkey_backup_lock);
				/*
				* Keylocker spec 0.76 Table 4-1:
				* 'Backup/restore valid' bit and 'IWKeyBackup consumed' bit
				*/
				vcpu->vm->arch_vm.iwkey_backup_status = 0x9UL;
				vcpu->arch.iwkey_copy_status = 1UL;
			}
		} else {
			err = -EINVAL;
		}
		break;
	}
	case MSR_IA32_COPY_PLATFORM_TO_LOCAL:
	{
		/* check feature support and avoid setting reserved MSR bits */
		if (is_iwkey_backup_support(vcpu) && ((v & ~0x1UL) == 0x0UL)) {
			if ((v == 0x1UL) && (vcpu->vm->arch_vm.iwkey_backup_status == 0x9UL)) {
				spinlock_obtain(&vcpu->vm->arch_vm.iwkey_backup_lock);
				vcpu->arch.IWKey = vcpu->vm->arch_vm.iwkey_backup;
				spinlock_release(&vcpu->vm->arch_vm.iwkey_backup_lock);
				/* Load the new iwkey for this vcpu */
				get_cpu_var(arch.whose_iwkey) = NULL;
				load_iwkey(vcpu);
				vcpu->arch.iwkey_copy_status = 1UL;
			}
		} else {
			err = -EINVAL;
		}
		break;
	}
	case MSR_IA32_UMWAIT_CONTROL:
	{
		/* Feature X86_FEATURE_WAITPKG is always presented */
		if (pcpu_has_cap(X86_FEATURE_WAITPKG)) {
			vcpu_set_guest_msr(vcpu, msr, v);
			msr_write(msr, v);
		} else {
			err = -EACCES;
		}
		break;
	}
	case MSR_TEST_CTL:
	{
		/* If VM has MSR_TEST_CTL, ignore write operation
		 * If don't have MSR_TEST_CTL, trigger #GP
		 */
		if (has_core_cap(CORE_CAP_SPLIT_LOCK) || has_core_cap(CORE_CAP_UC_LOCK)) {
			vcpu_set_guest_msr(vcpu, MSR_TEST_CTL, v);
			pr_warn("Ignore writting 0x%llx to MSR_TEST_CTL from VM%d", v, vcpu->vm->vm_id);
		} else {
			vcpu_inject_gp(vcpu, 0U);
		}
		break;
	}
#ifdef CONFIG_VCAT_ENABLED
	case MSR_IA32_L2_MASK_BASE ... (MSR_IA32_L2_MASK_BASE +  NUM_CAT_L2_MSRS - 1U):
	case MSR_IA32_L3_MASK_BASE ... (MSR_IA32_L3_MASK_BASE +  NUM_CAT_L3_MSRS - 1U):
	{
		err = write_vcbm(vcpu, msr, v);
		break;
	}
	case MSR_IA32_PQR_ASSOC:
	{
		err = write_vclosid(vcpu, v);
		break;
	}
#endif
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
	if (!is_vtm_configured(vcpu->vm)) {
		enable_msr_interception(msr_bitmap, MSR_IA32_EXT_APIC_LVT_THERMAL, INTERCEPT_READ_WRITE);
	}
	set_tsc_msr_interception(vcpu, exec_vmread64(VMX_TSC_OFFSET_FULL) != 0UL);
}
