/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* This is a template header file for nuc7i7dnb platform ACPI info definition
 * works when Kconfig of ENFORCE_VALIDATED_ACPI_INFO is disabled.
 * When ENFORCE_VALIDATED_ACPI_INFO is enabled, we should use
 * ./misc/acrn-config/target/board_parser.py running on target
 * to generate nuc7i7dnb specific acpi info file named as nuc7i7dnb_acpi_info.h
 * and put it in hypervisor/arch/x86/configs/nuc7i7dnb/.
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

#define WAKE_VECTOR_32          0x7FA22F8CUL
#define WAKE_VECTOR_64          0x7FA22F98UL

#define RESET_REGISTER_ADDRESS  0xCF9UL
#define RESET_REGISTER_SPACE_ID SPACE_SYSTEM_IO
#define RESET_REGISTER_VALUE    0x6U

/* DRHD of DMAR */

#define DRHD_COUNT              2U

#define DRHD0_DEV_CNT           1U
#define DRHD0_SEGMENT           0U
#define DRHD0_FLAGS             0U
#define DRHD0_REG_BASE          0xFED90000UL
#define DRHD0_IGNORE            false
#define DRHD0_DEVSCOPE0_BUS     0x0U
#define DRHD0_DEVSCOPE0_PATH    0x10U
#define DRHD0_DEVSCOPE1_BUS     0x0U
#define DRHD0_DEVSCOPE1_PATH    0x0U
#define DRHD0_DEVSCOPE2_BUS     0x0U
#define DRHD0_DEVSCOPE2_PATH    0x0U
#define DRHD0_DEVSCOPE3_BUS     0x0U
#define DRHD0_DEVSCOPE3_PATH    0x0U

#define DRHD1_DEV_CNT           2U
#define DRHD1_SEGMENT           0U
#define DRHD1_FLAGS             1U
#define DRHD1_REG_BASE          0xFED91000UL
#define DRHD1_IGNORE            false
#define DRHD1_DEVSCOPE0_BUS     0xf0U
#define DRHD1_DEVSCOPE0_PATH    0xf8U
#define DRHD1_DEVSCOPE1_BUS     0x0U
#define DRHD1_DEVSCOPE1_PATH    0xf8U
#define DRHD1_DEVSCOPE2_BUS     0x0U
#define DRHD1_DEVSCOPE2_PATH    0x0U
#define DRHD1_DEVSCOPE3_BUS     0x0U
#define DRHD1_DEVSCOPE3_PATH    0x0U
#define DRHD1_IOAPIC_ID         2U

#define DRHD2_DEV_CNT           0U
#define DRHD2_SEGMENT           0U
#define DRHD2_FLAGS             0U
#define DRHD2_REG_BASE          0x00UL
#define DRHD2_IGNORE            false
#define DRHD2_DEVSCOPE0_BUS     0x0U
#define DRHD2_DEVSCOPE0_PATH    0x0U
#define DRHD2_DEVSCOPE1_BUS     0x0U
#define DRHD2_DEVSCOPE1_PATH    0x0U
#define DRHD2_DEVSCOPE2_BUS     0x0U
#define DRHD2_DEVSCOPE2_PATH    0x0U
#define DRHD2_DEVSCOPE3_BUS     0x0U
#define DRHD2_DEVSCOPE3_PATH    0x0U

#define DRHD3_DEV_CNT           0U
#define DRHD3_SEGMENT           0U
#define DRHD3_FLAGS             0U
#define DRHD3_REG_BASE          0x00UL
#define DRHD3_IGNORE            false
#define DRHD3_DEVSCOPE0_BUS     0x0U
#define DRHD3_DEVSCOPE0_PATH    0x0U
#define DRHD3_DEVSCOPE1_BUS     0x0U
#define DRHD3_DEVSCOPE1_PATH    0x0U
#define DRHD3_DEVSCOPE2_BUS     0x0U
#define DRHD3_DEVSCOPE2_PATH    0x0U
#define DRHD3_DEVSCOPE3_BUS     0x0U
#define DRHD3_DEVSCOPE3_PATH    0x0U

#endif /* PLATFORM_ACPI_INFO_H */
