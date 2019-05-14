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
#define DRHD_COUNT		0U

#define DRHD0_DEV_CNT		0U
#define DRHD0_SEGMENT		0U
#define DRHD0_FLAGS		0U
#define DRHD0_REG_BASE		0UL
#define DRHD0_IGNORE		false
#define DRHD0_DEVSCOPE0_BUS	0U
#define DRHD0_DEVSCOPE0_PATH	0U
#define DRHD0_DEVSCOPE1_BUS	0U
#define DRHD0_DEVSCOPE1_PATH	0U
#define DRHD0_DEVSCOPE2_BUS	0U
#define DRHD0_DEVSCOPE2_PATH	0U
#define DRHD0_DEVSCOPE3_BUS	0U
#define DRHD0_DEVSCOPE3_PATH	0U

#define DRHD1_DEV_CNT		0U
#define DRHD1_SEGMENT		0U
#define DRHD1_FLAGS		0U
#define DRHD1_REG_BASE		0UL
#define DRHD1_IGNORE		false
#define DRHD1_DEVSCOPE0_BUS	0U
#define DRHD1_DEVSCOPE0_PATH	0U
#define DRHD1_DEVSCOPE1_BUS	0U
#define DRHD1_DEVSCOPE1_PATH	0U
#define DRHD1_DEVSCOPE2_BUS	0U
#define DRHD1_DEVSCOPE2_PATH	0U
#define DRHD1_DEVSCOPE3_BUS	0U
#define DRHD1_DEVSCOPE3_PATH	0U
#define DRHD1_IOAPIC_ID		0U

#define DRHD2_DEV_CNT		0U
#define DRHD2_SEGMENT		0U
#define DRHD2_FLAGS		0U
#define DRHD2_REG_BASE		0U
#define DRHD2_IGNORE		false
#define DRHD2_DEVSCOPE0_BUS	0U
#define DRHD2_DEVSCOPE0_PATH	0U
#define DRHD2_DEVSCOPE1_BUS	0U
#define DRHD2_DEVSCOPE1_PATH	0U
#define DRHD2_DEVSCOPE2_BUS	0U
#define DRHD2_DEVSCOPE2_PATH	0U
#define DRHD2_DEVSCOPE3_BUS	0U
#define DRHD2_DEVSCOPE3_PATH	0U

#define DRHD3_DEV_CNT		0U
#define DRHD3_SEGMENT		0U
#define DRHD3_FLAGS		0U
#define DRHD3_REG_BASE		0U
#define DRHD3_IGNORE		false
#define DRHD3_DEVSCOPE0_BUS	0U
#define DRHD3_DEVSCOPE0_PATH	0U
#define DRHD3_DEVSCOPE1_BUS	0U
#define DRHD3_DEVSCOPE1_PATH	0U
#define DRHD3_DEVSCOPE2_BUS	0U
#define DRHD3_DEVSCOPE2_PATH	0U
#define DRHD3_DEVSCOPE3_BUS	0U
#define DRHD3_DEVSCOPE3_PATH	0U

#endif	/* PLATFORM_ACPI_INFO_H */
