/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <board.h>
#include <msr.h>
#include <vtd.h>
#include <pci.h>

#ifndef CONFIG_ACPI_PARSE_ENABLED
#error "DMAR info is not available, please set ACPI_PARSE_ENABLED to y in Kconfig. \
	Or use acrn-config tool to generate platform DMAR info."
#endif

struct dmar_info plat_dmar_info;

#ifdef CONFIG_RDT_ENABLED
struct platform_clos_info platform_l3_clos_array[MAX_PLATFORM_CLOS_NUM];
struct platform_clos_info platform_l2_clos_array[MAX_PLATFORM_CLOS_NUM] = {
	{
		.clos_mask = 0xff,
		.msr_index = MSR_IA32_L2_MASK_BASE,
	},
	{
		.clos_mask = 0xff,
		.msr_index = MSR_IA32_L2_MASK_BASE + 1U,
	},
	{
		.clos_mask = 0xff,
		.msr_index = MSR_IA32_L2_MASK_BASE + 2U,
	},
	{
		.clos_mask = 0xff,
		.msr_index = MSR_IA32_L2_MASK_BASE + 3U,
	},
};
struct platform_clos_info platform_mba_clos_array[MAX_PLATFORM_CLOS_NUM];
#endif

const struct cpu_state_table board_cpu_state_tbl;

const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM] = {
	{
		.bits.b = 0x0,
		.bits.d = 0xd,
		.bits.f = 0x0,
	},
};
const struct vmsix_on_msi_info vmsix_on_msi_devs[MAX_VMSIX_ON_MSI_PDEVS_NUM];
