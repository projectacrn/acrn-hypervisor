/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
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

/* add test here */

/* PCI mmcfg base of MCFG, pre-assumption is platform only has one PCI segment group */
#define DEFAULT_PCI_MMCFG_BASE	0xE0000000UL

/* DRHD of DMAR */
#define DRHD_COUNT		8U

#endif	/* PLATFORM_ACPI_INFO_H */
