/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* DO NOT MODIFY THIS FILE UNLESS YOU KNOW WHAT YOU ARE DOING!
 */

#ifndef PLATFORM_ACPI_INFO_H
#define PLATFORM_ACPI_INFO_H

/*
 * BIOS Information
 * Vendor: Intel Corp.
 * Version: DNKBLi7v.86A.0065.2019.0611.1424
 * Release Date: 06/11/2019
 * BIOS Revision: 5.6
 *
 * Base Board Information
 * Manufacturer: Intel Corporation
 * Product Name: NUC7i7DNB
 * Version: J83500-204
 */

/* pm sstate data */
#define PM1A_EVT_ADDRESS        0x1800UL
#define PM1A_EVT_ACCESS_SIZE    0x2U
#define PM1A_CNT_ADDRESS        0x1804UL

#define WAKE_VECTOR_32          0x8AA09F8CUL
#define WAKE_VECTOR_64          0x8AA09F98UL

#define RESET_REGISTER_ADDRESS  0xCF9UL
#define RESET_REGISTER_SPACE_ID SPACE_SYSTEM_IO
#define RESET_REGISTER_VALUE    0x6U

/* DRHD of DMAR */
#define DRHD_COUNT              2U

#define DRHD0_DEV_CNT           0x1U
#define DRHD0_SEGMENT           0x0U
#define DRHD0_FLAGS             0x0U
#define DRHD0_REG_BASE          0xFED90000UL
#define DRHD0_IGNORE            true
#define DRHD0_DEVSCOPE0_TYPE    0x1U
#define DRHD0_DEVSCOPE0_ID      0x0U
#define DRHD0_DEVSCOPE0_BUS     0x0U
#define DRHD0_DEVSCOPE0_PATH    0x10U

#define DRHD1_DEV_CNT           0x2U
#define DRHD1_SEGMENT           0x0U
#define DRHD1_FLAGS             0x1U
#define DRHD1_REG_BASE          0xFED91000UL
#define DRHD1_IGNORE            false
#define DRHD1_DEVSCOPE0_TYPE    0x3U
#define DRHD1_DEVSCOPE0_ID      0x2U
#define DRHD1_DEVSCOPE0_BUS     0xf0U
#define DRHD1_DEVSCOPE0_PATH    0xf8U
#define DRHD1_DEVSCOPE1_TYPE    0x4U
#define DRHD1_DEVSCOPE1_ID      0x0U
#define DRHD1_DEVSCOPE1_BUS     0x0U
#define DRHD1_DEVSCOPE1_PATH    0xf8U

/* PCI mmcfg base of MCFG */
#define DEFAULT_PCI_MMCFG_BASE   0xe0000000UL

#endif /* PLATFORM_ACPI_INFO_H */
