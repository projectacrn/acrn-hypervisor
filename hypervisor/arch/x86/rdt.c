/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/cpu.h>
#include <asm/cpu_caps.h>
#include <asm/cpufeatures.h>
#include <asm/cpuid.h>
#include <errno.h>
#include <logmsg.h>
#include <asm/rdt.h>
#include <asm/lib/bits.h>
#include <asm/board.h>
#include <asm/vm_config.h>
#include <asm/msr.h>

const uint16_t hv_clos = 0U;
/* RDT features can support different numbers of CLOS. Set the lowest numerical
 * clos value (common_num_closids - 1) that is common between the resources as
 * each resource's clos max value to have consistent allocation.
 */
#ifdef CONFIG_RDT_ENABLED
static uint16_t common_num_closids = HV_SUPPORTED_MAX_CLOS;

static struct rdt_info res_cap_info[RDT_NUM_RESOURCES] = {
	[RDT_RESOURCE_L3] = {
		.res.cache = {
			.bitmask = 0U,
			.cbm_len = 0U,
			.msr_qos_cfg = MSR_IA32_L3_QOS_CFG,
		},
		.num_closids = 0U,
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
		.num_closids = 0U,
		.res_id = RDT_RESID_L2,
		.msr_base = MSR_IA32_L2_MASK_BASE,
		.platform_clos_array = platform_l2_clos_array,
	},
	[RDT_RESOURCE_MBA] = {
		.res.membw = {
			.mba_max = 0U,
			.delay_linear = true,
		},
		.num_closids = 0U,
		.res_id = RDT_RESID_MBA,
		.msr_base = MSR_IA32_MBA_MASK_BASE,
		.platform_clos_array = platform_mba_clos_array,
	},
};

/*
 * @pre res == RDT_RESOURCE_L3 || res == RDT_RESOURCE_L2 || res == RDT_RESOURCE_MBA
 */
const struct rdt_info *get_rdt_res_cap_info(int res)
{
	return &res_cap_info[res];
}

/*
 * @pre res < RDT_NUM_RESOURCES
 * @pre res_clos_info[i].mba_delay <= res_cap_info[res].res.membw.mba_max
 * @pre length of res_clos_info[i].clos_mask <= cbm_len && all 1's in clos_mask is continuous
 */
static void setup_res_clos_msr(uint16_t pcpu_id, uint16_t res, struct platform_clos_info *res_clos_info)
{
	uint16_t i, mask_array_size = common_num_closids;
	uint32_t msr_index;
	uint64_t val;

	if (res != RDT_RESOURCE_MBA && res_cap_info[res].res.cache.is_cdp_enabled) {
		mask_array_size = mask_array_size << 1U;

		/* enable CDP before setting COS to simplify CAT mask remapping
		 * and prevent unintended behavior.
		 */
		msr_write(res_cap_info[res].res.cache.msr_qos_cfg, 0x1UL);
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
		/* If num_closids == 0, the resource is not supported
		 * so skip setting up resource MSR.
		 */
		if (res_cap_info[i].num_closids > 0U) {
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

	if ((res_cap_info[RDT_RESOURCE_L3].num_closids > 0U) ||
	    (res_cap_info[RDT_RESOURCE_L2].num_closids > 0U) ||
	    (res_cap_info[RDT_RESOURCE_MBA].num_closids > 0U)) {
		ret = true;
	}

	return ret;
}
#else
uint64_t clos2pqr_msr(__unused uint16_t clos)
{
	return 0UL;
}

bool is_platform_rdt_capable(void)
{
	return false;
}
#endif
