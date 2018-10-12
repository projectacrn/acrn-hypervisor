/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* This is a template header file for platform ACPI info definition,
 * we should use a user space tool running on target to generate this file.
 */
#ifndef PLATFORM_ACPI_INFO_H
#define PLATFORM_ACPI_INFO_H

#define ACPI_INFO_VALIDATED

/* pm sstate data */
#define PM1A_EVT_SPACE_ID	SPACE_SYSTEM_IO
#define PM1A_EVT_BIT_WIDTH	0x20U
#define PM1A_EVT_BIT_OFFSET	0U
#define PM1A_EVT_ACCESS_SIZE	3U
#define PM1A_EVT_ADDRESS	0x400UL

#define PM1B_EVT_SPACE_ID	SPACE_SYSTEM_IO
#define PM1B_EVT_BIT_WIDTH	0U
#define PM1B_EVT_BIT_OFFSET	0U
#define PM1B_EVT_ACCESS_SIZE	0U
#define PM1B_EVT_ADDRESS	0UL

#define PM1A_CNT_SPACE_ID	SPACE_SYSTEM_IO
#define PM1A_CNT_BIT_WIDTH	0x10U
#define PM1A_CNT_BIT_OFFSET	0U
#define PM1A_CNT_ACCESS_SIZE	2U
#define PM1A_CNT_ADDRESS	0x404UL

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

#define WAKE_VECTOR_32		0x7A86BBDCUL
#define WAKE_VECTOR_64		0x7A86BBE8UL

#endif	/* PLATFORM_ACPI_INFO_H */
