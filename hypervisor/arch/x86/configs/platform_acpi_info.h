/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* This is a template header file for platform ACPI info definition,
 * we should use a user space tool running on target to generate
 * board specific acpi info file named as $(CONFIG_BOARD)_acpi_info.h
 * and put it in hypervisor/arch/x86/configs/$(CONFIG_BOARD)/.
 */
#ifndef PLATFORM_ACPI_INFO_H
#define PLATFORM_ACPI_INFO_H

/* pm sstate data */
#define PM1A_EVT_ACCESS_SIZE	0U
#define PM1A_EVT_ADDRESS	0UL
#define PM1A_CNT_ADDRESS	0UL

#define WAKE_VECTOR_32		0UL
#define WAKE_VECTOR_64		0UL

#ifndef CONFIG_DMAR_PARSE_ENABLED
#error "The template platform_acpi_info.h would not include DRHD info MACROs, if your Kconfig disabled \
ACPI DMA Remapping tables parsing, please include DRHD info MACROs in your board specific platform_acpi_info.h \
and put the file under hypervisor/arch/x86/configs/($CONFIG_BOARD)/."
#endif

#endif	/* PLATFORM_ACPI_INFO_H */
