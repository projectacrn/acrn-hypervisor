/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * BIOS Information
 * Vendor: Intel Corporation
 * Version: EHLSFWI1.R00.2224.A00.2005281500
 * Release Date: 05/28/2020
 *
 * Base Board Information
 * Manufacturer: Intel Corporation
 * Product Name: ElkhartLake LPDDR4x T3 CRB
 * Version: 2
 */

#include <board.h>
#include <vtd.h>
#include <msr.h>
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

static struct dmar_dev_scope drhd2_dev_scope[DRHD2_DEV_CNT] = {
	{
		.type   = DRHD2_DEVSCOPE0_TYPE,
		.id     = DRHD2_DEVSCOPE0_ID,
		.bus    = DRHD2_DEVSCOPE0_BUS,
		.devfun = DRHD2_DEVSCOPE0_PATH,
	},
	{
		.type   = DRHD2_DEVSCOPE1_TYPE,
		.id     = DRHD2_DEVSCOPE1_ID,
		.bus    = DRHD2_DEVSCOPE1_BUS,
		.devfun = DRHD2_DEVSCOPE1_PATH,
	},
	{
		.type   = DRHD2_DEVSCOPE2_TYPE,
		.id     = DRHD2_DEVSCOPE2_ID,
		.bus    = DRHD2_DEVSCOPE2_BUS,
		.devfun = DRHD2_DEVSCOPE2_PATH,
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
	{
		.dev_cnt       = DRHD2_DEV_CNT,
		.segment       = DRHD2_SEGMENT,
		.flags         = DRHD2_FLAGS,
		.reg_base_addr = DRHD2_REG_BASE,
		.ignore        = DRHD2_IGNORE,
		.devices       = drhd2_dev_scope
	},
};

struct dmar_info plat_dmar_info = {
	.drhd_count = DRHD_COUNT,
	.drhd_units = drhd_info_array,
};

#ifdef CONFIG_RDT_ENABLED
struct platform_clos_info platform_l2_clos_array[MAX_CACHE_CLOS_NUM_ENTRIES];
struct platform_clos_info platform_l3_clos_array[MAX_CACHE_CLOS_NUM_ENTRIES];
struct platform_clos_info platform_mba_clos_array[MAX_MBA_CLOS_NUM_ENTRIES];
#endif

static const struct acrn_cstate_data board_cpu_cx[3] = {
	{{SPACE_FFixedHW, 0x00U, 0x00U, 0x00U, 0x00UL}, 0x01U, 0x01U, 0x00U},	/* C1 */
	{{SPACE_SYSTEM_IO, 0x08U, 0x00U, 0x00U, 0x1816UL}, 0x02U, 0xFDU, 0x00U},	/* C2 */
	{{SPACE_SYSTEM_IO, 0x08U, 0x00U, 0x00U, 0x1819UL}, 0x03U, 0x418U, 0x00U},	/* C3 */
};

static const struct acrn_pstate_data board_cpu_px[2] = {
	{0x5DDUL, 0x00UL, 0x0AUL, 0x0AUL, 0x000F00UL, 0x000F00UL},	/* P0 */
	{0x5DCUL, 0x00UL, 0x0AUL, 0x0AUL, 0x000F00UL, 0x000F00UL},	/* P1 */
};

const struct cpu_state_table board_cpu_state_tbl = {
	"Genuine Intel(R) CPU 0000 @ 1.50GHz",
	{(uint8_t)ARRAY_SIZE(board_cpu_px), board_cpu_px,
	(uint8_t)ARRAY_SIZE(board_cpu_cx), board_cpu_cx}
};
const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM];

#define VMSIX_ON_MSI_DEV0	.bdf.bits = {.b = 0x00U, .d = 0x1eU, .f =0x4U},
#define VMSIX_ON_MSI_DEV1	.bdf.bits = {.b = 0x00U, .d = 0x1dU, .f =0x1U},
#define VMSIX_ON_MSI_DEV2	.bdf.bits = {.b = 0x00U, .d = 0x1dU, .f =0x2U},
#define VMSIX_ON_MSI_DEV3	.bdf.bits = {.b = 0x00U, .d = 0x13U, .f =0x4U},
#define VMSIX_ON_MSI_DEV4	.bdf.bits = {.b = 0x00U, .d = 0x13U, .f =0x5U},
const struct vmsix_on_msi_info vmsix_on_msi_devs[MAX_VMSIX_ON_MSI_PDEVS_NUM] = {
	{VMSIX_ON_MSI_DEV0},
	{VMSIX_ON_MSI_DEV1},
	{VMSIX_ON_MSI_DEV2},
	{VMSIX_ON_MSI_DEV3},
	{VMSIX_ON_MSI_DEV4},
};
