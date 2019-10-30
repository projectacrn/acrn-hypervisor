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

#define RESET_REGISTER_ADDRESS	0UL
#define RESET_REGISTER_VALUE	0UL
#define RESET_REGISTER_SPACE_ID 0UL

/* DRHD of DMAR */
#define DRHD_COUNT		8U

#endif	/* PLATFORM_ACPI_INFO_H */
