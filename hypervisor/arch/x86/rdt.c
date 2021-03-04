/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <x86/cpu.h>
#include <x86/cpu_caps.h>
#include <x86/cpufeatures.h>
#include <x86/cpuid.h>
#include <errno.h>
#include <logmsg.h>
#include <x86/rdt.h>
#include <x86/lib/bits.h>
#include <x86/board.h>
#include <x86/vm_config.h>
#include <x86/msr.h>

const uint16_t hv_clos = 0U;
/* RDT features can support different numbers of CLOS. Set the lowers numerical
 * clos value (valid_clos_num) that is common between the resources as
 * each resource's clos max value to have consistent allocation.
 */
#ifdef CONFIG_RDT_ENABLED
uint16_t valid_clos_num = HV_SUPPORTED_MAX_CLOS;

static struct rdt_info res_cap_info[RDT_NUM_RESOURCES] = {
	[RDT_RESOURCE_L3] = {
		.res.cache = {
			.bitmask = 0U,
			.cbm_len = 0U,
			.msr_qos_cfg = MSR_IA32_L3_QOS_CFG,
		},
		.clos_max = 0U,
		.res_id = RDT_RESID_L3,
		.msr_base = MSR_IA32_L3_MASK_BASE,
		.platform_clos_array = platform_l3_clos_array,
	},
	[RDT_RESOURCE_L2] = {
		.res.cache = {
			.bitmask = 0U,
			.cbm_len = 0U,
			.msr_qos_cfg = MSR_IA32_L2_QOS_CFG,
		},
		.clos_max = 0U,
		.res_id = RDT_RESID_L2,
		.msr_base = MSR_IA32_L2_MASK_BASE,
		.platform_clos_array = platform_l2_clos_array,
	},
	[RDT_RESOURCE_MBA] = {
		.res.membw = {
			.mba_max = 0U,
			.delay_linear = true,
		},
		.clos_max = 0U,
		.res_id = RDT_RESID_MBA,
		.msr_base = MSR_IA32_MBA_MASK_BASE,
		.platform_clos_array = platform_mba_clos_array,
	},
};

/*
 * @pre res == RDT_RESOURCE_L3 || res == RDT_RESOURCE_L2
 */
static void init_cat_capability(int res)
{
	uint32_t eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;

	/* CPUID.(EAX=0x10,ECX=ResID):EAX[4:0] reports the length of CBM supported
	 * CPUID.(EAX=0x10,ECX=ResID):EBX[31:0] indicates shared cache mask bits
	 * that are used by other entities such as graphic and H/W outside processor.
	 * CPUID.(EAX=0x10,ECX=ResID):EDX[15:0] reports the maximun CLOS supported
	 */
	cpuid_subleaf(CPUID_RDT_ALLOCATION, res_cap_info[res].res_id, &eax, &ebx, &ecx, &edx);
	res_cap_info[res].res.cache.cbm_len = (uint16_t)((eax & 0x1fU) + 1U);
	res_cap_info[res].res.cache.bitmask = ebx;
#ifdef CONFIG_CDP_ENABLED
	res_cap_info[res].res.cache.is_cdp_enabled = ((ecx & 0x4U) != 0U);
#else
	res_cap_info[res].res.cache.is_cdp_enabled = false;
#endif
	if (res_cap_info[res].res.cache.is_cdp_enabled) {
		res_cap_info[res].clos_max = (uint16_t)((edx & 0xffffU) >> 1U) + 1U;
		/* enable CDP before setting COS to simplify CAT mask remapping
		 * and prevent unintended behavior.
		 */
		msr_write(res_cap_info[res].res.cache.msr_qos_cfg, 0x1UL);
	} else {
		res_cap_info[res].clos_max = (uint16_t)(edx & 0xffffU) + 1U;
	}
}

static void init_mba_capability(int res)
{
	uint32_t eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;

	/* CPUID.(EAX=0x10,ECX=ResID):EAX[11:0] reports maximum MBA throttling value supported
	 * CPUID.(EAX=0x10,ECX=ResID):EBX[31:0] reserved
	 * CPUID.(EAX=10H, ECX=ResID=3):ECX[2] reports if response of the delay values is linear
	 * CPUID.(EAX=0x10,ECX=ResID):EDX[15:0] reports the maximun CLOS supported
	 */
	cpuid_subleaf(CPUID_RDT_ALLOCATION, res_cap_info[res].res_id, &eax, &ebx, &ecx, &edx);
	res_cap_info[res].res.membw.mba_max = (uint16_t)((eax & 0xfffU) + 1U);
	res_cap_info[res].res.membw.delay_linear = ((ecx & 0x4U) != 0U);
	res_cap_info[res].clos_max = (uint16_t)(edx & 0xffffU) + 1U;
}

/*
 * @pre valid_clos_num > 0U
 */
void init_rdt_info(void)
{
	uint8_t i;
	uint32_t eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;

	if (pcpu_has_cap(X86_FEATURE_RDT_A)) {
		cpuid_subleaf(CPUID_RDT_ALLOCATION, 0U, &eax, &ebx, &ecx, &edx);

		/* If HW supports L3 CAT, EBX[1] is set */
		if ((ebx & 2U) != 0U) {
			init_cat_capability(RDT_RESOURCE_L3);
		}

		/* If HW supports L2 CAT, EBX[2] is set */
		if ((ebx & 4U) != 0U) {
			init_cat_capability(RDT_RESOURCE_L2);
		}

		/* If HW supports MBA, EBX[3] is set */
		if ((ebx & 8U) != 0U) {
			init_mba_capability(RDT_RESOURCE_MBA);
		}

		for (i = 0U; i < RDT_NUM_RESOURCES; i++) {
			/* If clos_max == 0, the resource is not supported. Set the
			 * valid_clos_num as the minimal clos_max of all support rdt resource.
			 */
			if (res_cap_info[i].clos_max > 0U) {
				if (res_cap_info[i].clos_max < valid_clos_num) {
					valid_clos_num = res_cap_info[i].clos_max;
				}
			}
		}
	}
}

/*
 * @pre res < RDT_NUM_RESOURCES
 * @pre res_clos_info[i].mba_delay <= res_cap_info[res].res.membw.mba_max
 * @pre length of res_clos_info[i].clos_mask <= cbm_len && all 1's in clos_mask is continuous
 */
static void setup_res_clos_msr(uint16_t pcpu_id, uint16_t res, struct platform_clos_info *res_clos_info)
{
	uint16_t i, mask_array_size = valid_clos_num;
	uint32_t msr_index;
	uint64_t val;

	if (res != RDT_RESOURCE_MBA && res_cap_info[res].res.cache.is_cdp_enabled) {
		mask_array_size = mask_array_size << 1U;
	}
	for (i = 0U; i < mask_array_size; i++) {
		switch (res) {
		case RDT_RESOURCE_L3:
		case RDT_RESOURCE_L2:
			val = (uint64_t)res_clos_info[i].value.clos_mask;
			break;
		case RDT_RESOURCE_MBA:
			val = (uint64_t)res_clos_info[i].value.mba_delay;
			break;
		default:
			ASSERT(res < RDT_NUM_RESOURCES, "Support only 3 RDT resources. res=%d is invalid", res);
		}
		msr_index = res_cap_info[res].msr_base + i;
		msr_write_pcpu(msr_index, val, pcpu_id);
	}
}

void setup_clos(uint16_t pcpu_id)
{
	uint16_t i;

	for (i = 0U; i < RDT_NUM_RESOURCES; i++) {
		/* If clos_max == 0, the resource is not supported
		 * so skip setting up resource MSR.
		 */
		if (res_cap_info[i].clos_max > 0U) {
			setup_res_clos_msr(pcpu_id, i, res_cap_info[i].platform_clos_array);
		}
	}

	/* set hypervisor RDT resource clos */
	msr_write_pcpu(MSR_IA32_PQR_ASSOC, clos2pqr_msr(hv_clos), pcpu_id);
}

uint64_t clos2pqr_msr(uint16_t clos)
{
	uint64_t pqr_assoc;

	pqr_assoc = msr_read(MSR_IA32_PQR_ASSOC);
	pqr_assoc = (pqr_assoc & 0xffffffffUL) | ((uint64_t)clos << 32U);

	return pqr_assoc;
}

bool is_platform_rdt_capable(void)
{
	bool ret = false;

	if ((res_cap_info[RDT_RESOURCE_L3].clos_max > 0U) ||
	    (res_cap_info[RDT_RESOURCE_L2].clos_max > 0U) ||
	    (res_cap_info[RDT_RESOURCE_MBA].clos_max > 0U)) {
		ret = true;
	}

	return ret;
}
#else
uint64_t clos2pqr_msr(uint16_t clos)
{
	(void)(clos);
	return 0UL;
}

bool is_platform_rdt_capable(void)
{
	return false;
}
#endif
