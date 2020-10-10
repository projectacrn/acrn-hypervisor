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
 * Vendor: INSYDE Corp.
 * Version: Z01-0001A027
 * Release Date: 10/14/2019
 * BIOS Revision: 1.28
 *
 * Base Board Information
 * Manufacturer: Logic Supply
 * Product Name: RXM-181
 * Version: Type2 - Board Version
 */

/* pm sstate data */
#define PM1A_EVT_ADDRESS        0x1800UL
#define PM1A_EVT_ACCESS_SIZE    0x2U
#undef PM1B_EVT_SPACE_ID
#define PM1B_EVT_SPACE_ID       SPACE_SYSTEM_MEMORY
#define PM1A_CNT_ADDRESS        0x1804UL
#undef PM1B_CNT_SPACE_ID
#define PM1B_CNT_SPACE_ID       SPACE_SYSTEM_MEMORY

#define WAKE_VECTOR_32          0x8BB2F00CUL
#define WAKE_VECTOR_64          0x8BB2F018UL

#define RESET_REGISTER_ADDRESS  0xB2UL
#define RESET_REGISTER_SPACE_ID SPACE_SYSTEM_IO
#define RESET_REGISTER_VALUE    0xfbU

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
#define DRHD1_DEVSCOPE0_BUS     0x0U
#define DRHD1_DEVSCOPE0_PATH    0xf7U
#define DRHD1_DEVSCOPE1_TYPE    0x4U
#define DRHD1_DEVSCOPE1_ID      0x0U
#define DRHD1_DEVSCOPE1_BUS     0x0U
#define DRHD1_DEVSCOPE1_PATH    0xf6U

/* PCI mmcfg base of MCFG */
#define DEFAULT_PCI_MMCFG_BASE   0xe0000000UL

/* PCI mmcfg bus number of MCFG */
#define DEFAULT_PCI_MMCFG_START_BUS 	 0x0U
#define DEFAULT_PCI_MMCFG_END_BUS   	 0xFFU


#endif /* PLATFORM_ACPI_INFO_H */
