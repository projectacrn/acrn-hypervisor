/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#ifndef CONFIG_DMAR_PARSE_ENABLED
static struct dmar_dev_scope default_drhd_unit_dev_scope0[] = {
	{ .bus = 0U, .devfun = DEVFUN(0x2U, 0U), },
};

static struct dmar_drhd drhd_info_array[] = {
	{
		.dev_cnt = 1U,
		.segment = 0U,
		.flags = 0U,
		.reg_base_addr = 0xFED64000UL,
		/* Ignore the iommu for intel graphic device since GVT-g needs
		 * vtd disabled for gpu
		 */
		.ignore = true,
		.devices = default_drhd_unit_dev_scope0,
	},
	{
		/* No need to specify devices since
		 * DRHD_FLAG_INCLUDE_PCI_ALL_MASK set
		 */
		.dev_cnt = 0U,
		.segment = 0U,
		.flags = DRHD_FLAG_INCLUDE_PCI_ALL_MASK,
		.reg_base_addr = 0xFED65000UL,
		.ignore = false,
		.devices = NULL,
	},
};

static struct dmar_info sbl_dmar_info = {
	.drhd_count = 2,
	.drhd_units = drhd_info_array,
};

struct dmar_info *get_dmar_info(void)
{
	return &sbl_dmar_info;
}
#endif

void    init_bsp(void)
{
#ifndef CONFIG_CONSTANT_ACPI
	acpi_fixup();
#endif
}
