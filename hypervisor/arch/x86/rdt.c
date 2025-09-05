/*
 * Copyright (C) 2020-2025 Intel Corporation.
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
#include <bits.h>
#include <asm/board.h>
#include <asm/vm_config.h>
#include <asm/msr.h>

const uint16_t hv_clos = 0U;
/* RDT features can support different numbers of CLOS. Set the lowest numerical
 * clos value (common_num_closids - 1) that is common between the resources as
 * each resource's clos max value to have consistent allocation.
 */
#ifdef CONFIG_RDT_ENABLED

/*
 * @pre res == RDT_RESOURCE_L3 || res == RDT_RESOURCE_L2 || res == RDT_RESOURCE_MBA
 */
const struct rdt_ins *get_rdt_res_ins(int res, uint16_t pcpu_id)
{
	uint32_t i;
	struct rdt_type *info = &res_cap_info[res];
	struct rdt_ins *ins = NULL;

	for (i = 0U; i < info->num_ins; i++) {
		if (bitmap_test(pcpu_id, &info->ins_array[i].cpu_mask)) {
			ins = &info->ins_array[i];
			break;
		}
	}
	return ins;
}

static void setup_res_clos_msr(uint16_t pcpu_id, struct rdt_type *info, struct rdt_ins *ins)
{
	uint16_t i;
	uint32_t msr_index;
	uint64_t val = 0;
	uint32_t res = info->res_id;
	union clos_config *cfg = ins->clos_config_array;

	if (res != RDT_RESID_MBA && ins->res.cache.is_cdp_enabled) {
		/* enable CDP before setting COS to simplify CAT mask remapping
		 * and prevent unintended behavior.
		 */
		msr_write(info->msr_qos_cfg, 0x1UL);
	}

	for (i = 0U; i < ins->num_clos_config; i++) {
		switch (res) {
		case RDT_RESOURCE_L3:
		case RDT_RESOURCE_L2:
			val = (uint64_t)cfg[i].clos_mask;
			break;
		case RDT_RESOURCE_MBA:
			val = (uint64_t)cfg[i].mba_delay;
			break;
		default:
			ASSERT(res < RDT_NUM_RESOURCES, "Support only 3 RDT resources. res=%d is invalid", res);
		}
		msr_index = info->msr_base + i;
		msr_write_pcpu(msr_index, val, pcpu_id);
	}
}

void setup_clos(uint16_t pcpu_id)
{
	uint16_t i, j;
	struct rdt_type *info;
	struct rdt_ins *ins;

	for (i = 0U; i < RDT_NUM_RESOURCES; i++) {
		info = &res_cap_info[i];
		for (j = 0U; j < info->num_ins; j++) {
			ins = &info->ins_array[j];
			if (bitmap_test(pcpu_id, &ins->cpu_mask)) {
				setup_res_clos_msr(pcpu_id, info, ins);
			}
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

static bool is_rdt_type_capable(struct rdt_type *info)
{
	uint32_t i;
	struct rdt_ins *ins;
	bool ret = false;

	if (info->num_ins > 0U) {
		for (i = 0U; i < info->num_ins; i++) {
			ins = &info->ins_array[i];
			if (ins->num_closids > 0U) {
				ret = true;
				break;
			}
		}
	}

	return ret;
}

bool is_platform_rdt_capable(void)
{
	bool ret = false;

	if (is_rdt_type_capable(&res_cap_info[RDT_RESOURCE_L3]) ||
	    is_rdt_type_capable(&res_cap_info[RDT_RESOURCE_L2]) ||
	    is_rdt_type_capable(&res_cap_info[RDT_RESOURCE_MBA])) {
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
