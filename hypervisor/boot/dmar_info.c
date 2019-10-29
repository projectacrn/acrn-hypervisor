/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vtd.h>
#include <platform_acpi_info.h>

static struct dmar_dev_scope drhd0_dev_scope[MAX_DRHD_DEVSCOPES] = {
	{
		.bus = DRHD0_DEVSCOPE0_BUS,
		.devfun = DRHD0_DEVSCOPE0_PATH
	},
	{
		.bus = DRHD0_DEVSCOPE1_BUS,
		.devfun = DRHD0_DEVSCOPE1_PATH
	},
	{
		.bus = DRHD0_DEVSCOPE2_BUS,
		.devfun = DRHD0_DEVSCOPE2_PATH
	},
	{
		.bus = DRHD0_DEVSCOPE3_BUS,
		.devfun = DRHD0_DEVSCOPE3_PATH
	}
};

static struct dmar_dev_scope drhd1_dev_scope[MAX_DRHD_DEVSCOPES] = {
	{
		.type = ACPI_DMAR_SCOPE_TYPE_IOAPIC,
		.id = DRHD1_IOAPIC_ID,
		.bus = DRHD1_DEVSCOPE0_BUS,
		.devfun = DRHD1_DEVSCOPE0_PATH
	},
	{
		.bus = DRHD1_DEVSCOPE1_BUS,
		.devfun = DRHD1_DEVSCOPE1_PATH
	},
	{
		.bus = DRHD1_DEVSCOPE2_BUS,
		.devfun = DRHD1_DEVSCOPE2_PATH
	},
	{
		.bus = DRHD1_DEVSCOPE3_BUS,
		.devfun = DRHD1_DEVSCOPE3_PATH
	}
};

static struct dmar_drhd drhd_info_array[MAX_DRHDS] = {
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

static struct dmar_info plat_dmar_info = {
	.drhd_count = DRHD_COUNT,
	.drhd_units = drhd_info_array,
};

/**
 * @post return != NULL
 * @post return->drhd_count > 0U
 */
struct dmar_info *get_dmar_info(void)
{
#ifdef CONFIG_ACPI_PARSE_ENABLED
	parse_dmar_table(&plat_dmar_info);
#endif
	return &plat_dmar_info;
}
