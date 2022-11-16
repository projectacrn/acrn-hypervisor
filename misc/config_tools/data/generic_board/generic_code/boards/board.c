/*
 * Copyright (C) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * BIOS Information
 * Vendor: American Megatrends International, LLC.
 * Version: E5000XXU3F00105-BPCIe
 * Release Date: 08/25/2021
 * BIOS Revision: 5.19
 *
 * Base Board Information
 * Manufacturer: Default string
 * Product Name: Default string
 * Version: Default string
 */

#include <asm/board.h>
#include <asm/vtd.h>
#include <asm/msr.h>
#include <asm/rdt.h>
#include <pci.h>
#include <misc_cfg.h>

static struct dmar_dev_scope drhd0_dev_scope[DRHD0_DEV_CNT] = {
	{
		.type   = DRHD0_DEVSCOPE0_TYPE,
		.id     = DRHD0_DEVSCOPE0_ID,
		.bus    = DRHD0_DEVSCOPE0_BUS,
		.devfun = DRHD0_DEVSCOPE0_PATH,
	},
};

static struct dmar_dev_scope drhd1_dev_scope[DRHD1_DEV_CNT] = {
	{
		.type   = DRHD1_DEVSCOPE0_TYPE,
		.id     = DRHD1_DEVSCOPE0_ID,
		.bus    = DRHD1_DEVSCOPE0_BUS,
		.devfun = DRHD1_DEVSCOPE0_PATH,
	},
	{
		.type   = DRHD1_DEVSCOPE1_TYPE,
		.id     = DRHD1_DEVSCOPE1_ID,
		.bus    = DRHD1_DEVSCOPE1_BUS,
		.devfun = DRHD1_DEVSCOPE1_PATH,
	},
};

static struct dmar_drhd drhd_info_array[DRHD_COUNT] = {
	{
		.dev_cnt       = DRHD0_DEV_CNT,
		.segment       = DRHD0_SEGMENT,
		.flags         = DRHD0_FLAGS,
		.reg_base_addr = DRHD0_REG_BASE,
		.ignore        = DRHD0_IGNORE,
		.devices       = drhd0_dev_scope
	},
	{
		.dev_cnt       = DRHD1_DEV_CNT,
		.segment       = DRHD1_SEGMENT,
		.flags         = DRHD1_FLAGS,
		.reg_base_addr = DRHD1_REG_BASE,
		.ignore        = DRHD1_IGNORE,
		.devices       = drhd1_dev_scope
	},
};

struct dmar_info plat_dmar_info = {
	.drhd_count = DRHD_COUNT,
	.drhd_units = drhd_info_array,
};

#ifdef CONFIG_RDT_ENABLED
union clos_config platform_l2_clos_array_0[8] = {
};

union clos_config platform_l2_clos_array_1[8] = {
};

union clos_config platform_l2_clos_array_2[8] = {
};

union clos_config platform_l2_clos_array_3[8] = {
};

union clos_config platform_l3_clos_array[MAX_CACHE_CLOS_NUM_ENTRIES];
union clos_config platform_mba_clos_array[MAX_MBA_CLOS_NUM_ENTRIES];
struct rdt_ins rdt_ins_l2[4] = {
	{
		.res.cache = {
			.bitmask = 0xfffff,
			.cbm_len = 20,
			.is_cdp_enabled = false,
		},
		.num_closids = 8,
		.num_clos_config = 0,
		.clos_config_array = platform_l2_clos_array_0,
		.cpu_mask = 0x1,
	},
	{
		.res.cache = {
			.bitmask = 0xfffff,
			.cbm_len = 20,
			.is_cdp_enabled = false,
		},
		.num_closids = 8,
		.num_clos_config = 0,
		.clos_config_array = platform_l2_clos_array_1,
		.cpu_mask = 0x2,
	},
	{
		.res.cache = {
			.bitmask = 0xfffff,
			.cbm_len = 20,
			.is_cdp_enabled = false,
		},
		.num_closids = 8,
		.num_clos_config = 0,
		.clos_config_array = platform_l2_clos_array_2,
		.cpu_mask = 0x4,
	},
	{
		.res.cache = {
			.bitmask = 0xfffff,
			.cbm_len = 20,
			.is_cdp_enabled = false,
		},
		.num_closids = 8,
		.num_clos_config = 0,
		.clos_config_array = platform_l2_clos_array_3,
		.cpu_mask = 0x8,
	},
};

struct rdt_type res_cap_info[RDT_NUM_RESOURCES] = {
	{
		.res_id = RDT_RESID_L2,
		.msr_qos_cfg = MSR_IA32_L2_QOS_CFG,
		.msr_base = MSR_IA32_L2_MASK_BASE,
		.num_ins = 4,
		.ins_array = rdt_ins_l2,
	},
};

#endif

static const struct acrn_cstate_data board_cpu_cx[1] = {
	{{SPACE_FFixedHW, 0x00U, 0x00U, 0x00U, 0x00UL}, 0x01U, 0x00U, 0x00U},	/* C1 */
};

/* Px data is not available */
static const struct acrn_pstate_data board_cpu_px[0];

const struct cpu_state_table board_cpu_state_tbl = {
	"11th Gen Intel(R) Core(TM) i7-1185GRE @ 2.80GHz",
	{(uint8_t)ARRAY_SIZE(board_cpu_px), board_cpu_px,
	(uint8_t)ARRAY_SIZE(board_cpu_cx), board_cpu_cx}
};
const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM];

const struct vmsix_on_msi_info vmsix_on_msi_devs[MAX_VMSIX_ON_MSI_PDEVS_NUM];

struct acrn_cpufreq_limits cpufreq_limits[MAX_PCPU_NUM] = {
	{
		.guaranteed_hwp_lvl = 255,
		.highest_hwp_lvl = 255,
		.lowest_hwp_lvl = 1,
		.nominal_pstate = 0,
		.performance_pstate = 0,
	},
	{
		.guaranteed_hwp_lvl = 255,
		.highest_hwp_lvl = 255,
		.lowest_hwp_lvl = 1,
		.nominal_pstate = 0,
		.performance_pstate = 0,
	},
	{
		.guaranteed_hwp_lvl = 255,
		.highest_hwp_lvl = 255,
		.lowest_hwp_lvl = 1,
		.nominal_pstate = 0,
		.performance_pstate = 0,
	},
	{
		.guaranteed_hwp_lvl = 255,
		.highest_hwp_lvl = 255,
		.lowest_hwp_lvl = 1,
		.nominal_pstate = 0,
		.performance_pstate = 0,
	},
};
