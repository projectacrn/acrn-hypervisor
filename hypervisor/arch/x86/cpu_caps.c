/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <msr.h>
#include <page.h>
#include <cpufeatures.h>
#include <cpuid.h>
#include <spinlock.h>
#include <cpu.h>
#include <per_cpu.h>
#include <vmx.h>
#include <cpu_caps.h>
#include <errno.h>
#include <logmsg.h>
#include <vmcs.h>

/* TODO: add more capability per requirement */
/* APICv features */
#define VAPIC_FEATURE_VIRT_ACCESS		(1U << 0U)
#define VAPIC_FEATURE_VIRT_REG			(1U << 1U)
#define VAPIC_FEATURE_INTR_DELIVERY		(1U << 2U)
#define VAPIC_FEATURE_TPR_SHADOW		(1U << 3U)
#define VAPIC_FEATURE_POST_INTR			(1U << 4U)
#define VAPIC_FEATURE_VX2APIC_MODE		(1U << 5U)

static struct cpu_capability {
	uint8_t apicv_features;
	uint8_t ept_features;

	uint32_t vmx_ept;
	uint32_t vmx_vpid;
} cpu_caps;

static struct cpuinfo_x86 boot_cpu_data;

bool cpu_has_cap(uint32_t bit)
{
	uint32_t feat_idx = bit >> 5U;
	uint32_t feat_bit = bit & 0x1fU;
	bool ret;

	if (feat_idx >= FEATURE_WORDS) {
		ret = false;
	} else {
		ret = ((boot_cpu_data.cpuid_leaves[feat_idx] & (1U << feat_bit)) != 0U);
	}

	return ret;
}

bool has_monitor_cap(void)
{
	bool ret = false;

	if (cpu_has_cap(X86_FEATURE_MONITOR)) {
		/* don't use monitor for CPU (family: 0x6 model: 0x5c)
		 * in hypervisor, but still expose it to the guests and
		 * let them handle it correctly
		 */
		if ((boot_cpu_data.family != 0x6U) || (boot_cpu_data.model != 0x5cU)) {
			ret = true;
		}
	}

	return ret;
}

static inline bool is_fast_string_erms_supported_and_enabled(void)
{
	bool ret = false;
	uint32_t misc_enable = (uint32_t)msr_read(MSR_IA32_MISC_ENABLE);

	if ((misc_enable & MSR_IA32_MISC_ENABLE_FAST_STRING) == 0U) {
		pr_fatal("%s, fast string is not enabled\n", __func__);
	} else {
		if (!cpu_has_cap(X86_FEATURE_ERMS)) {
			pr_fatal("%s, enhanced rep movsb/stosb not supported\n", __func__);
		} else {
			ret = true;
		}
	}

	return ret;
}

/*check allowed ONEs setting in vmx control*/
static bool is_ctrl_setting_allowed(uint64_t msr_val, uint32_t ctrl)
{
	/*
	 * Intel SDM Appendix A.3
	 * - bitX in ctrl can be set 1
	 *   only if bit 32+X in msr_val is 1
	 */
	return ((((uint32_t)(msr_val >> 32UL)) & ctrl) == ctrl);
}

static void detect_ept_cap(void)
{
	uint64_t msr_val;

	cpu_caps.ept_features = 0U;

	/* Read primary processor based VM control. */
	msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS);

	/*
	 * According to SDM A.3.2 Primary Processor-Based VM-Execution Controls:
	 * The IA32_VMX_PROCBASED_CTLS MSR (index 482H) reports on the allowed
	 * settings of most of the primary processor-based VM-execution controls
	 * (see Section 24.6.2):
	 * Bits 63:32 indicate the allowed 1-settings of these controls.
	 * VM entry allows control X to be 1 if bit 32+X in the MSR is set to 1;
	 * if bit 32+X in the MSR is cleared to 0, VM entry fails if control X
	 * is 1.
	 */
	msr_val = msr_val >> 32U;

	/* Check if secondary processor based VM control is available. */
	if ((msr_val & VMX_PROCBASED_CTLS_SECONDARY) != 0UL) {
		/* Read secondary processor based VM control. */
		msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS2);

		if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_EPT)) {
			cpu_caps.ept_features = 1U;
		}
	}
}

static void detect_apicv_cap(void)
{
	uint8_t features = 0U;
	uint64_t msr_val;

	msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS);
	/* must support TPR shadow */
	if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS_TPR_SHADOW)) {
		features |= VAPIC_FEATURE_TPR_SHADOW;

		msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS2);
		/* must support APICV access */
		if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VAPIC)) {
			features |= VAPIC_FEATURE_VIRT_ACCESS;
			if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VAPIC_REGS)) {
				features |= VAPIC_FEATURE_VIRT_REG;

				if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VX2APIC)) {
					features |= VAPIC_FEATURE_VX2APIC_MODE;
				}

				if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VIRQ)) {
					features |= VAPIC_FEATURE_INTR_DELIVERY;

					msr_val = msr_read(MSR_IA32_VMX_PINBASED_CTLS);
					if (is_ctrl_setting_allowed(msr_val, VMX_PINBASED_CTLS_POST_IRQ)) {
						features |= VAPIC_FEATURE_POST_INTR;
					}
				}
				cpu_caps.apicv_features = features;
			} else {
				/* platform may only support APICV access */
				cpu_caps.apicv_features = features;
			}
		}
	}
}

static void detect_vmx_mmu_cap(void)
{
	uint64_t val;

	/* Read the MSR register of EPT and VPID Capability -  SDM A.10 */
	val = msr_read(MSR_IA32_VMX_EPT_VPID_CAP);
	cpu_caps.vmx_ept = (uint32_t) val;
	cpu_caps.vmx_vpid = (uint32_t) (val >> 32U);
}

static void detect_cpu_cap(void)
{
	detect_apicv_cap();
	detect_ept_cap();
	detect_vmx_mmu_cap();
}

static uint64_t get_address_mask(uint8_t limit)
{
	return ((1UL << limit) - 1UL) & PAGE_MASK;
}

void init_cpu_capabilities(void)
{
	uint32_t eax, unused;
	uint32_t family, model;

	cpuid(CPUID_VENDORSTRING,
		&boot_cpu_data.cpuid_level,
		&unused, &unused, &unused);

	cpuid(CPUID_FEATURES, &eax, &unused,
		&boot_cpu_data.cpuid_leaves[FEAT_1_ECX],
		&boot_cpu_data.cpuid_leaves[FEAT_1_EDX]);
	family = (eax >> 8U) & 0xffU;
	if (family == 0xFU) {
		family += (eax >> 20U) & 0xffU;
	}
	boot_cpu_data.family = (uint8_t)family;

	model = (eax >> 4U) & 0xfU;
	if (family >= 0x06U) {
		model += ((eax >> 16U) & 0xfU) << 4U;
	}
	boot_cpu_data.model = (uint8_t)model;


	cpuid(CPUID_EXTEND_FEATURE, &unused,
		&boot_cpu_data.cpuid_leaves[FEAT_7_0_EBX],
		&boot_cpu_data.cpuid_leaves[FEAT_7_0_ECX],
		&boot_cpu_data.cpuid_leaves[FEAT_7_0_EDX]);

	cpuid(CPUID_MAX_EXTENDED_FUNCTION,
		&boot_cpu_data.extended_cpuid_level,
		&unused, &unused, &unused);

	if (boot_cpu_data.extended_cpuid_level >= CPUID_EXTEND_FUNCTION_1) {
		cpuid(CPUID_EXTEND_FUNCTION_1, &unused, &unused,
			&boot_cpu_data.cpuid_leaves[FEAT_8000_0001_ECX],
			&boot_cpu_data.cpuid_leaves[FEAT_8000_0001_EDX]);
	}

	if (boot_cpu_data.extended_cpuid_level >= CPUID_EXTEND_ADDRESS_SIZE) {
		cpuid(CPUID_EXTEND_ADDRESS_SIZE, &eax,
			&boot_cpu_data.cpuid_leaves[FEAT_8000_0008_EBX],
			&unused, &unused);

			/* EAX bits 07-00: #Physical Address Bits
			 *     bits 15-08: #Linear Address Bits
			 */
			boot_cpu_data.virt_bits = (uint8_t)((eax >> 8U) & 0xffU);
			boot_cpu_data.phys_bits = (uint8_t)(eax & 0xffU);
			boot_cpu_data.physical_address_mask =
				get_address_mask(boot_cpu_data.phys_bits);
	}

	detect_cpu_cap();
}

static bool is_ept_supported(void)
{
	return (cpu_caps.ept_features != 0U);
}

static bool is_apicv_supported(void)
{
	return (cpu_caps.apicv_features != 0U);
}

bool is_apicv_reg_virtualization_supported(void)
{
	return ((cpu_caps.apicv_features & VAPIC_FEATURE_VIRT_REG) != 0U);
}

bool is_apicv_intr_delivery_supported(void)
{
	return ((cpu_caps.apicv_features & VAPIC_FEATURE_INTR_DELIVERY) != 0U);
}

bool is_apicv_posted_intr_supported(void)
{
	return ((cpu_caps.apicv_features & VAPIC_FEATURE_POST_INTR) != 0U);
}

bool cpu_has_vmx_ept_cap(uint32_t bit_mask)
{
	return ((cpu_caps.vmx_ept & bit_mask) != 0U);
}

bool cpu_has_vmx_vpid_cap(uint32_t bit_mask)
{
	return ((cpu_caps.vmx_vpid & bit_mask) != 0U);
}

void init_cpu_model_name(void)
{
	cpuid(CPUID_EXTEND_FUNCTION_2,
		(uint32_t *)(boot_cpu_data.model_name),
		(uint32_t *)(&boot_cpu_data.model_name[4]),
		(uint32_t *)(&boot_cpu_data.model_name[8]),
		(uint32_t *)(&boot_cpu_data.model_name[12]));
	cpuid(CPUID_EXTEND_FUNCTION_3,
		(uint32_t *)(&boot_cpu_data.model_name[16]),
		(uint32_t *)(&boot_cpu_data.model_name[20]),
		(uint32_t *)(&boot_cpu_data.model_name[24]),
		(uint32_t *)(&boot_cpu_data.model_name[28]));
	cpuid(CPUID_EXTEND_FUNCTION_4,
		(uint32_t *)(&boot_cpu_data.model_name[32]),
		(uint32_t *)(&boot_cpu_data.model_name[36]),
		(uint32_t *)(&boot_cpu_data.model_name[40]),
		(uint32_t *)(&boot_cpu_data.model_name[44]));

	boot_cpu_data.model_name[48] = '\0';
}

static inline bool is_vmx_disabled(void)
{
	uint64_t msr_val;
	bool ret = false;

	/* Read Feature ControL MSR */
	msr_val = msr_read(MSR_IA32_FEATURE_CONTROL);

	/* Check if feature control is locked and vmx cannot be enabled */
	if (((msr_val & MSR_IA32_FEATURE_CONTROL_LOCK) != 0U) &&
		((msr_val & MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX) == 0U)) {
		ret = true;
	}

	return ret;
}

static inline bool cpu_has_vmx_unrestricted_guest_cap(void)
{
	return ((msr_read(MSR_IA32_VMX_MISC) & VMX_SUPPORT_UNRESTRICTED_GUEST)
									!= 0UL);
}

static int32_t check_vmx_mmu_cap(void)
{
	int32_t ret = 0;

	if (!cpu_has_vmx_ept_cap(VMX_EPT_INVEPT)) {
		pr_fatal("%s, invept not supported\n", __func__);
		ret = -ENODEV;
	} else if (!cpu_has_vmx_vpid_cap(VMX_VPID_INVVPID) ||
		!cpu_has_vmx_vpid_cap(VMX_VPID_INVVPID_SINGLE_CONTEXT) ||
		!cpu_has_vmx_vpid_cap(VMX_VPID_INVVPID_GLOBAL_CONTEXT)) {
		pr_fatal("%s, invvpid not supported\n", __func__);
		ret = -ENODEV;
	} else if (!cpu_has_vmx_ept_cap(VMX_EPT_1GB_PAGE)) {
		pr_fatal("%s, ept not support 1GB large page\n", __func__);
		ret = -ENODEV;
	}

	return ret;
}


/*
 * basic hardware capability check
 * we should supplement which feature/capability we must support
 * here later.
 */
int32_t detect_hardware_support(void)
{
	int32_t ret;

	/* Long Mode (x86-64, 64-bit support) */
	if (!cpu_has_cap(X86_FEATURE_LM)) {
		pr_fatal("%s, LM not supported\n", __func__);
		ret = -ENODEV;
	} else if ((boot_cpu_data.phys_bits == 0U) ||
		(boot_cpu_data.virt_bits == 0U)) {
		pr_fatal("%s, can't detect Linear/Physical Address size\n", __func__);
		ret = -ENODEV;
	} else if (!cpu_has_cap(X86_FEATURE_TSC_DEADLINE)) {
		/* lapic TSC deadline timer */
		pr_fatal("%s, TSC deadline not supported\n", __func__);
		ret = -ENODEV;
	} else if (!cpu_has_cap(X86_FEATURE_NX)) {
		/* Execute Disable */
		pr_fatal("%s, NX not supported\n", __func__);
		ret = -ENODEV;
	} else if (!cpu_has_cap(X86_FEATURE_SMEP)) {
		/* Supervisor-Mode Execution Prevention */
		pr_fatal("%s, SMEP not supported\n", __func__);
		ret = -ENODEV;
	} else if (!cpu_has_cap(X86_FEATURE_SMAP)) {
		/* Supervisor-Mode Access Prevention */
		pr_fatal("%s, SMAP not supported\n", __func__);
		ret = -ENODEV;
	} else if (!cpu_has_cap(X86_FEATURE_MTRR)) {
		pr_fatal("%s, MTRR not supported\n", __func__);
		ret = -ENODEV;
	} else if (!cpu_has_cap(X86_FEATURE_PAGE1GB)) {
		pr_fatal("%s, not support 1GB page\n", __func__);
		ret = -ENODEV;
	} else if (!cpu_has_cap(X86_FEATURE_VMX)) {
		pr_fatal("%s, vmx not supported\n", __func__);
		ret = -ENODEV;
	} else if (!is_fast_string_erms_supported_and_enabled()) {
		ret = -ENODEV;
	} else if (!cpu_has_vmx_unrestricted_guest_cap()) {
		pr_fatal("%s, unrestricted guest not supported\n", __func__);
		ret = -ENODEV;
	} else if (!is_ept_supported()) {
		pr_fatal("%s, EPT not supported\n", __func__);
		ret = -ENODEV;
	} else if (!is_apicv_supported()) {
		pr_fatal("%s, APICV not supported\n", __func__);
		ret = -ENODEV;
	} else if (boot_cpu_data.cpuid_level < 0x15U) {
		pr_fatal("%s, required CPU feature not supported\n", __func__);
		ret = -ENODEV;
	} else if (is_vmx_disabled()) {
		pr_fatal("%s, VMX can not be enabled\n", __func__);
		ret = -ENODEV;
	} else if (get_pcpu_nums() > CONFIG_MAX_PCPU_NUM) {
		pr_fatal("%s, pcpu number(%d) is out of range\n", __func__, get_pcpu_nums());
		ret = -ENODEV;
	} else {
		ret = check_vmx_mmu_cap();
		if (ret == 0) {
			pr_acrnlog("hardware support HV");
		}
	}

	return ret;
}

struct cpuinfo_x86 *get_cpu_info(void)
{
	return &boot_cpu_data;
}
