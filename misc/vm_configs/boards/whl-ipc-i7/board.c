/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * BIOS Information
 * Vendor: American Megatrends Inc.
 * Version: WL10R104
 * Release Date: 09/12/2019
 * BIOS Revision: 5.13
 *
 * Base Board Information
 * Manufacturer: Maxtang
 * Product Name: WL10
 * Version: V1.0
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
struct platform_clos_info platform_l2_clos_array[MAX_PLATFORM_CLOS_NUM];
struct platform_clos_info platform_l3_clos_array[MAX_PLATFORM_CLOS_NUM];
struct platform_clos_info platform_mba_clos_array[MAX_PLATFORM_CLOS_NUM];
#endif

static const struct cpu_cx_data board_cpu_cx[3] = {
	{{SPACE_FFixedHW, 0x01U, 0x02U, 0x01U, 0x00UL}, 0x01U, 0x01U, 0x00U},	/* C1 */
	{{SPACE_FFixedHW, 0x01U, 0x02U, 0x01U, 0x33UL}, 0x02U, 0x97U, 0x00U},	/* C2 */
	{{SPACE_FFixedHW, 0x01U, 0x02U, 0x01U, 0x60UL}, 0x03U, 0x40AU, 0x00U},	/* C3 */
};

static const struct cpu_px_data board_cpu_px[10] = {
	{0x835UL, 0x00UL, 0x0AUL, 0x0AUL, 0x002E00UL, 0x002E00UL},	/* P0 */
	{0x834UL, 0x00UL, 0x0AUL, 0x0AUL, 0x001500UL, 0x001500UL},	/* P1 */
	{0x7D0UL, 0x00UL, 0x0AUL, 0x0AUL, 0x001400UL, 0x001400UL},	/* P2 */
	{0x76CUL, 0x00UL, 0x0AUL, 0x0AUL, 0x001300UL, 0x001300UL},	/* P3 */
	{0x708UL, 0x00UL, 0x0AUL, 0x0AUL, 0x001200UL, 0x001200UL},	/* P4 */
	{0x6A4UL, 0x00UL, 0x0AUL, 0x0AUL, 0x001100UL, 0x001100UL},	/* P5 */
	{0x640UL, 0x00UL, 0x0AUL, 0x0AUL, 0x001000UL, 0x001000UL},	/* P6 */
	{0x5DCUL, 0x00UL, 0x0AUL, 0x0AUL, 0x000F00UL, 0x000F00UL},	/* P7 */
	{0x578UL, 0x00UL, 0x0AUL, 0x0AUL, 0x000E00UL, 0x000E00UL},	/* P8 */
	{0x514UL, 0x00UL, 0x0AUL, 0x0AUL, 0x000D00UL, 0x000D00UL},	/* P9 */
};

const struct cpu_state_table board_cpu_state_tbl = {
	"Intel(R) Core(TM) i7-8565U CPU @ 1.80GHz",
	{(uint8_t)ARRAY_SIZE(board_cpu_px), board_cpu_px,
	(uint8_t)ARRAY_SIZE(board_cpu_cx), board_cpu_cx}
};
const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM];

const struct vmsix_on_msi_info vmsix_on_msi_devs[MAX_VMSIX_ON_MSI_PDEVS_NUM];
