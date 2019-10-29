/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <board.h>
#include <vtd.h>

#ifndef CONFIG_ACPI_PARSE_ENABLED
#error "DMAR info is not available, please set ACPI_PARSE_ENABLED to y in Kconfig. \
	Or use acrn-config tool to generate platform DMAR info."
#endif

struct dmar_info plat_dmar_info;
struct platform_clos_info platform_clos_array[0];
uint16_t platform_clos_num = 0;
