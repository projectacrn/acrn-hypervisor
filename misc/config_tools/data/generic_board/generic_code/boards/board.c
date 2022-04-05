/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * BIOS Information
 * Vendor: Intel Corp.
 * Version: TNTGL357.0042.2020.1221.1743
 * Release Date: 12/21/2020
 * BIOS Revision: 5.19
 *
 * Base Board Information
 * Manufacturer: Intel Corporation
 * Product Name: NUC11TNBi5
 * Version: M11904-403
 */

#include <asm/board.h>
#include <asm/vtd.h>
#include <asm/msr.h>
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
};

static struct dmar_dev_scope drhd2_dev_scope[DRHD2_DEV_CNT] = {
	{
		.type   = DRHD2_DEVSCOPE0_TYPE,
		.id     = DRHD2_DEVSCOPE0_ID,
		.bus    = DRHD2_DEVSCOPE0_BUS,
		.devfun = DRHD2_DEVSCOPE0_PATH,
	},
};

static struct dmar_dev_scope drhd3_dev_scope[DRHD3_DEV_CNT] = {
	{
		.type   = DRHD3_DEVSCOPE0_TYPE,
		.id     = DRHD3_DEVSCOPE0_ID,
		.bus    = DRHD3_DEVSCOPE0_BUS,
		.devfun = DRHD3_DEVSCOPE0_PATH,
	},
	{
		.type   = DRHD3_DEVSCOPE1_TYPE,
		.id     = DRHD3_DEVSCOPE1_ID,
		.bus    = DRHD3_DEVSCOPE1_BUS,
		.devfun = DRHD3_DEVSCOPE1_PATH,
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
	{
		.dev_cnt       = DRHD3_DEV_CNT,
		.segment       = DRHD3_SEGMENT,
		.flags         = DRHD3_FLAGS,
		.reg_base_addr = DRHD3_REG_BASE,
		.ignore        = DRHD3_IGNORE,
		.devices       = drhd3_dev_scope
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

static const struct acrn_pstate_data board_cpu_px[16] = {
	{0x961UL, 0x00UL, 0x0AUL, 0x0AUL, 0x002A00UL, 0x002A00UL},	/* P0 */
	{0x960UL, 0x00UL, 0x0AUL, 0x0AUL, 0x001800UL, 0x001800UL},	/* P1 */
	{0x8FCUL, 0x00UL, 0x0AUL, 0x0AUL, 0x001700UL, 0x001700UL},	/* P2 */
	{0x834UL, 0x00UL, 0x0AUL, 0x0AUL, 0x001500UL, 0x001500UL},	/* P3 */
	{0x7D0UL, 0x00UL, 0x0AUL, 0x0AUL, 0x001400UL, 0x001400UL},	/* P4 */
	{0x708UL, 0x00UL, 0x0AUL, 0x0AUL, 0x001200UL, 0x001200UL},	/* P5 */
	{0x6A4UL, 0x00UL, 0x0AUL, 0x0AUL, 0x001100UL, 0x001100UL},	/* P6 */
	{0x5DCUL, 0x00UL, 0x0AUL, 0x0AUL, 0x000F00UL, 0x000F00UL},	/* P7 */
	{0x578UL, 0x00UL, 0x0AUL, 0x0AUL, 0x000E00UL, 0x000E00UL},	/* P8 */
	{0x514UL, 0x00UL, 0x0AUL, 0x0AUL, 0x000D00UL, 0x000D00UL},	/* P9 */
	{0x44CUL, 0x00UL, 0x0AUL, 0x0AUL, 0x000B00UL, 0x000B00UL},	/* P10 */
	{0x384UL, 0x00UL, 0x0AUL, 0x0AUL, 0x000900UL, 0x000900UL},	/* P11 */
	{0x320UL, 0x00UL, 0x0AUL, 0x0AUL, 0x000800UL, 0x000800UL},	/* P12 */
	{0x2BCUL, 0x00UL, 0x0AUL, 0x0AUL, 0x000700UL, 0x000700UL},	/* P13 */
	{0x1F4UL, 0x00UL, 0x0AUL, 0x0AUL, 0x000500UL, 0x000500UL},	/* P14 */
	{0x190UL, 0x00UL, 0x0AUL, 0x0AUL, 0x000400UL, 0x000400UL},	/* P15 */
};

const struct cpu_state_table board_cpu_state_tbl = {
	"11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz",
	{(uint8_t)ARRAY_SIZE(board_cpu_px), board_cpu_px,
	(uint8_t)ARRAY_SIZE(board_cpu_cx), board_cpu_cx}
};
const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM];

const struct vmsix_on_msi_info vmsix_on_msi_devs[MAX_VMSIX_ON_MSI_PDEVS_NUM];
