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
#include <board.h>
#include <vm_config.h>
#include <msr.h>

struct cat_hw_info cat_cap_info;
const uint16_t hv_clos = 0U;
static uint16_t platform_clos_num = MAX_PLATFORM_CLOS_NUM;

int32_t init_cat_cap_info(void)
{
	uint32_t eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;
	int32_t ret = 0;

	if (pcpu_has_cap(X86_FEATURE_CAT)) {
		cpuid_subleaf(CPUID_RSD_ALLOCATION, 0, &eax, &ebx, &ecx, &edx);
		/* If support L3 CAT, EBX[1] is set */
		if ((ebx & 2U) != 0U) {
			cat_cap_info.res_id = CAT_RESID_L3;
		}

		/* If support L2 CAT, EBX[2] is set */
		if ((ebx & 4U) != 0U) {
			cat_cap_info.res_id = CAT_RESID_L2;
		}

		cat_cap_info.enabled = true;

		/* CPUID.(EAX=0x10,ECX=ResID):EAX[4:0] reports the length of CBM supported
		 * CPUID.(EAX=0x10,ECX=ResID):EBX[31:0] indicates the corresponding uints
		 * may be used by other entities such as graphic and H/W outside processor.
		 *  CPUID.(EAX=0x10,ECX=ResID):EDX[15:0] reports the maximun CLOS supported
		 */
		cpuid_subleaf(CPUID_RSD_ALLOCATION, cat_cap_info.res_id, &eax, &ebx, &ecx, &edx);
		cat_cap_info.cbm_len = (uint16_t)((eax & 0x1fU) + 1U);
		cat_cap_info.bitmask = ebx;
		cat_cap_info.clos_max = (uint16_t)(edx & 0xffffU);

		if ((platform_clos_num != 0U) && ((cat_cap_info.clos_max + 1U) != platform_clos_num)) {
			pr_err("%s clos_max:%hu, platform_clos_num:%u\n", __func__, cat_cap_info.clos_max, platform_clos_num);
			ret = -EINVAL;
		}
	}

	return ret;
}


void setup_clos(uint16_t pcpu_id)
{
	uint16_t i;
	uint32_t msr_index;
	uint64_t val;

	if (cat_cap_info.enabled) {
		for (i = 0U; i < platform_clos_num; i++) {
			msr_index = platform_clos_array[i].msr_index;
			val = (uint64_t)platform_clos_array[i].clos_mask;
			msr_write_pcpu(msr_index, val, pcpu_id);
		}
		/* set hypervisor CAT clos */
		msr_write_pcpu(MSR_IA32_PQR_ASSOC, clos2prq_msr(hv_clos), pcpu_id);
	}
}

uint64_t clos2prq_msr(uint16_t clos)
{
	uint64_t prq_assoc;

	prq_assoc = msr_read(MSR_IA32_PQR_ASSOC);
	prq_assoc = (prq_assoc & 0xffffffffUL) | ((uint64_t)clos << 32U);

	return prq_assoc;
}
