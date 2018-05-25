/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

/* IOAPIC id */
#define SBL_IOAPIC_ID   8
/* IOAPIC base address */
#define SBL_IOAPIC_ADDR 0xfec00000
/* IOAPIC range size */
#define SBL_IOAPIC_SIZE 0x100000
/* Local APIC base address */
#define SBL_LAPIC_ADDR 0xfee00000
/* Local APIC range size */
#define SBL_LAPIC_SIZE 0x100000
/* Number of PCI IRQ assignments */
#define SBL_PCI_IRQ_ASSIGNMENT_NUM 28

#ifndef CONFIG_DMAR_PARSE_ENABLED
static struct dmar_dev_scope default_drhd_unit_dev_scope0[] = {
	{ .bus = 0, .devfun = DEVFUN(0x2, 0), },
};

static struct dmar_drhd drhd_info_array[] = {
	{
		.dev_cnt = 1,
		.segment = 0,
		.flags = 0,
		.reg_base_addr = 0xFED64000,
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
		.dev_cnt = 0,
		.segment = 0,
		.flags = DRHD_FLAG_INCLUDE_PCI_ALL_MASK,
		.reg_base_addr = 0xFED65000,
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
}
