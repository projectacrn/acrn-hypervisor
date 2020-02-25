/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <cpu.h>
#include <cpu_caps.h>
#include <cpufeatures.h>
#include <cpuid.h>
#include <errno.h>
#include <logmsg.h>
#include <rdt.h>
#include <bits.h>
#include <board.h>
#include <vm_config.h>
#include <msr.h>

static struct rdt_info res_cap_info[RDT_NUM_RESOURCES] = {
	[RDT_RESOURCE_L3] = {
		.bitmask = 0U,
		.cbm_len = 0U,
		.clos_max = 0U,
		.res_id = RDT_RESID_L3,
		.msr_base = MSR_IA32_L3_MASK_BASE,
		.platform_clos_array = NULL
	},
	[RDT_RESOURCE_L2] = {
		.bitmask = 0U,
		.cbm_len = 0U,
		.clos_max = 0U,
		.res_id = RDT_RESID_L2,
		.msr_base = MSR_IA32_L2_MASK_BASE,
		.platform_clos_array = NULL
	},
};
const uint16_t hv_clos = 0U;
/* RDT features can support different numbers of CLOS. Set  the lowers numerical
 * clos value (platform_clos_num) that is common between the resources as
 * each resource's clos max value to have consistent allocation.
 */
const uint16_t platform_clos_num = MAX_PLATFORM_CLOS_NUM;

#ifdef CONFIG_RDT_ENABLED
static void rdt_read_cat_capability(int res)
{
	uint32_t eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;

	/* CPUID.(EAX=0x10,ECX=ResID):EAX[4:0] reports the length of CBM supported
	 * CPUID.(EAX=0x10,ECX=ResID):EBX[31:0] indicates shared cache mask bits
	 * that are used by other entities such as graphic and H/W outside processor.
	 * CPUID.(EAX=0x10,ECX=ResID):EDX[15:0] reports the maximun CLOS supported
	 */
	cpuid_subleaf(CPUID_RDT_ALLOCATION, res_cap_info[res].res_id, &eax, &ebx, &ecx, &edx);
	res_cap_info[res].cbm_len = (uint16_t)((eax & 0xfU) + 1U);
	res_cap_info[res].bitmask = ebx;
	res_cap_info[res].clos_max = (uint16_t)(edx & 0xffffU) + 1;
}

int32_t init_rdt_cap_info(void)
{
	uint8_t i;
	uint32_t eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;
	int32_t ret = 0;

	if (pcpu_has_cap(X86_FEATURE_RDT_A)) {
		cpuid_subleaf(CPUID_RDT_ALLOCATION, 0, &eax, &ebx, &ecx, &edx);

		/* If HW supports L3 CAT, EBX[1] is set */
		if ((ebx & 2U) != 0U) {
			rdt_read_cat_capability(RDT_RESOURCE_L3);
		}

		/* If HW supports L2 CAT, EBX[2] is set */
		if ((ebx & 4U) != 0U) {
			rdt_read_cat_capability(RDT_RESOURCE_L2);
		}

		for (i = 0U; i < RDT_NUM_RESOURCES; i++) {
			/* If clos_max == 0, the resource is not supported
			 * so skip checking and updating the clos_max
			 */
			if (res_cap_info[i].clos_max > 0U) {
				if ((platform_clos_num == 0U) || (res_cap_info[i].clos_max < platform_clos_num)) {
					pr_err("Invalid Res_ID %d clos max:platform_clos_max=%d, res_clos_max=%d\n",
						res_cap_info[i].res_id, platform_clos_num, res_cap_info[i].clos_max);
					ret = -EINVAL;
					break;
				}
				/*Store user configured platform clos mask and MSR in the rdt_info struct*/
				if (res_cap_info[i].res_id == RDT_RESID_L3) {
					res_cap_info[i].platform_clos_array = platform_l3_clos_array;
				} else if (res_cap_info[i].res_id == RDT_RESID_L2) {
					res_cap_info[i].platform_clos_array = platform_l2_clos_array;
				} else {
					res_cap_info[i].platform_clos_array = NULL;
				}
			}
		}
	}
	return ret;
}

static bool setup_res_clos_msr(uint16_t pcpu_id, struct platform_clos_info *res_clos_info)
{
	bool ret = true;
	uint16_t i;
	uint32_t msr_index;
	uint64_t val;

	for (i = 0; i < platform_clos_num; i++) {
		if ((fls32(res_clos_info->clos_mask) >= res_cap_info->cbm_len) ||
			(res_clos_info->msr_index != (res_cap_info->msr_base + i))) {
			ret = false;
			pr_err("Incorrect CLOS %d Mask=0x%x and(/or) MSR index=0x%x for Res_ID %d in board.c",
				    i, res_clos_info->clos_mask, res_clos_info->msr_index, res_cap_info[i].res_id);
			break;
		}
		msr_index = res_clos_info->msr_index;
		val = (uint64_t)res_clos_info->clos_mask;
		msr_write_pcpu(msr_index, val, pcpu_id);
		res_clos_info++;
	}

	return ret;
}

bool setup_clos(uint16_t pcpu_id)
{
	bool ret = true;
	uint16_t i;

	for (i = 0U; i < RDT_NUM_RESOURCES; i++) {
		/* If clos_max == 0, the resource is not supported
		 * so skip setting up resource MSR.
		 */
		if (res_cap_info[i].clos_max > 0U) {
			ret = setup_res_clos_msr(pcpu_id, res_cap_info[i].platform_clos_array);
			if (!ret)
				break;
		}
	}

	if (ret) {
		/* set hypervisor RDT resource clos */
		msr_write_pcpu(MSR_IA32_PQR_ASSOC, clos2pqr_msr(hv_clos), pcpu_id);
	}

	return ret;
}

uint64_t clos2pqr_msr(uint16_t clos)
{
	uint64_t pqr_assoc;

	pqr_assoc = msr_read(MSR_IA32_PQR_ASSOC);
	pqr_assoc = (pqr_assoc & 0xffffffffUL) | ((uint64_t)clos << 32U);

	return pqr_assoc;
}
#else
uint64_t clos2pqr_msr(uint16_t clos)
{
	(void)(clos);
	return 0UL;
}
#endif

bool is_platform_rdt_capable(void)
{
	bool ret = false;

	if ((res_cap_info[RDT_RESOURCE_L3].clos_max > 0U) ||
	    (res_cap_info[RDT_RESOURCE_L2].clos_max > 0U)) {
		ret = true;
	}

	return ret;
}
