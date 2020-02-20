/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <board.h>
#include <vtd.h>
#include <pci.h>

#ifndef CONFIG_ACPI_PARSE_ENABLED
#error "DMAR info is not available, please set ACPI_PARSE_ENABLED to y in Kconfig. \
	Or use acrn-config tool to generate platform DMAR info."
#endif

struct dmar_info plat_dmar_info;
struct platform_clos_info platform_clos_array[MAX_PLATFORM_CLOS_NUM];
const struct cpu_state_table board_cpu_state_tbl;

const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM] = {
       {
               .bits.b = 0x0U,
               .bits.d = 0xdU,
               .bits.f = 0x0U,
       },
};
