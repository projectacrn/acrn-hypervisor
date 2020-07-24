/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <board.h>
#include <vtd.h>
#include <pci.h>

static struct dmar_dev_scope drhd0_dev_scope[DRHD0_DEV_CNT] = {
	{
		.type = DRHD0_DEVSCOPE0_TYPE,
		.id = DRHD0_DEVSCOPE0_ID,
		.bus = DRHD0_DEVSCOPE0_BUS,
		.devfun = DRHD0_DEVSCOPE0_PATH
	},
};

static struct dmar_dev_scope drhd1_dev_scope[DRHD1_DEV_CNT] = {
	{
		.type = DRHD1_DEVSCOPE0_TYPE,
		.id = DRHD1_DEVSCOPE0_ID,
		.bus = DRHD1_DEVSCOPE0_BUS,
		.devfun = DRHD1_DEVSCOPE0_PATH
	},
	{
		.type = DRHD1_DEVSCOPE1_TYPE,
		.id = DRHD1_DEVSCOPE1_ID,
		.bus = DRHD1_DEVSCOPE1_BUS,
		.devfun = DRHD1_DEVSCOPE1_PATH
	},
};

static struct dmar_drhd drhd_info_array[DRHD_COUNT] = {
	{
		.dev_cnt = DRHD0_DEV_CNT,
		.segment = DRHD0_SEGMENT,
		.flags = DRHD0_FLAGS,
		.reg_base_addr = DRHD0_REG_BASE,
		.ignore = DRHD0_IGNORE,
		.devices = drhd0_dev_scope
	},
	{
		.dev_cnt = DRHD1_DEV_CNT,
		.segment = DRHD1_SEGMENT,
		.flags = DRHD1_FLAGS,
		.reg_base_addr = DRHD1_REG_BASE,
		.ignore = DRHD1_IGNORE,
		.devices = drhd1_dev_scope
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

const struct cpu_state_table board_cpu_state_tbl;
const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM];
const struct vmsix_on_msi_info vmsix_on_msi_devs[MAX_VMSIX_ON_MSI_PDEVS_NUM];
