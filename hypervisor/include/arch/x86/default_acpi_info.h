/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Given the reality that some of ACPI configrations are unlikey changed,
 * define these MACROs in this header file.
 * The platform_acpi_info.h still has chance to override the default
 * definition by #undef with offline tool.
 */
#ifndef DEFAULT_ACPI_INFO_H
#define DEFAULT_ACPI_INFO_H

/* APIC */
#define LAPIC_BASE		0xFEE00000UL

#define IOAPIC0_BASE		0xFEC00000UL
#define IOAPIC1_BASE		0UL

/* pm sstate data */
#define PM1A_EVT_SPACE_ID	SPACE_SYSTEM_IO
#define PM1A_EVT_BIT_WIDTH	0x20U
#define PM1A_EVT_BIT_OFFSET	0U

#define PM1B_EVT_SPACE_ID	SPACE_SYSTEM_IO
#define PM1B_EVT_BIT_WIDTH	0U
#define PM1B_EVT_BIT_OFFSET	0U
#define PM1B_EVT_ACCESS_SIZE	0U
#define PM1B_EVT_ADDRESS	0UL

#define PM1A_CNT_SPACE_ID	SPACE_SYSTEM_IO
#define PM1A_CNT_BIT_WIDTH	0x10U
#define PM1A_CNT_BIT_OFFSET	0U
#define PM1A_CNT_ACCESS_SIZE	2U

#define PM1B_CNT_SPACE_ID	SPACE_SYSTEM_IO
#define PM1B_CNT_BIT_WIDTH	0U
#define PM1B_CNT_BIT_OFFSET	0U
#define PM1B_CNT_ACCESS_SIZE	0U
#define PM1B_CNT_ADDRESS	0UL

#define S3_PKG_VAL_PM1A		0x05U
#define S3_PKG_VAL_PM1B		0U
#define S3_PKG_RESERVED		0U

#define S5_PKG_VAL_PM1A		0x07U
#define S5_PKG_VAL_PM1B		0U
#define S5_PKG_RESERVED		0U

#endif	/* DEFAULT_ACPI_INFO_H */
