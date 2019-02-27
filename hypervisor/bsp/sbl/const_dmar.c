/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <vtd.h>

#ifndef CONFIG_DMAR_PARSE_ENABLED

#define MAX_DRHDS		4
#define MAX_DRHD_DEVSCOPES	4

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

static struct dmar_dev_scope drhd2_dev_scope[MAX_DRHD_DEVSCOPES] = {
	{
		.bus = DRHD2_DEVSCOPE0_BUS,
		.devfun = DRHD2_DEVSCOPE0_PATH
	},
	{
		.bus = DRHD2_DEVSCOPE1_BUS,
		.devfun = DRHD2_DEVSCOPE1_PATH
	},
	{
		.bus = DRHD2_DEVSCOPE2_BUS,
		.devfun = DRHD2_DEVSCOPE2_PATH
	},
	{
		.bus = DRHD2_DEVSCOPE3_BUS,
		.devfun = DRHD2_DEVSCOPE3_PATH
	}
};

static struct dmar_dev_scope drhd3_dev_scope[MAX_DRHD_DEVSCOPES] = {
	{
		.bus = DRHD3_DEVSCOPE0_BUS,
		.devfun = DRHD3_DEVSCOPE0_PATH
	},
	{
		.bus = DRHD3_DEVSCOPE1_BUS,
		.devfun = DRHD3_DEVSCOPE1_PATH
	},
	{
		.bus = DRHD3_DEVSCOPE2_BUS,
		.devfun = DRHD3_DEVSCOPE2_PATH
	},
	{
		.bus = DRHD3_DEVSCOPE3_BUS,
		.devfun = DRHD3_DEVSCOPE3_PATH
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
	{
		.dev_cnt = DRHD2_DEV_CNT,
		.segment = DRHD2_SEGMENT,
		.flags = DRHD2_FLAGS,
		.reg_base_addr = DRHD2_REG_BASE,
		.ignore = DRHD2_IGNORE,
		.devices = drhd2_dev_scope
	},
	{
		.dev_cnt = DRHD3_DEV_CNT,
		.segment = DRHD3_SEGMENT,
		.flags = DRHD3_FLAGS,
		.reg_base_addr = DRHD3_REG_BASE,
		.ignore = DRHD3_IGNORE,
		.devices = drhd3_dev_scope
	}
};

static struct dmar_info sbl_dmar_info = {
	.drhd_count = DRHD_COUNT,
	.drhd_units = drhd_info_array,
};

/**
 * @post return != NULL
 * @post return->drhd_count > 0U
 */
struct dmar_info *get_dmar_info(void)
{
	return &sbl_dmar_info;
}
#endif
