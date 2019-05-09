/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* This is a template header file for apl-mrb platform ACPI info definition,
 * we should use a user space tool running on target to generate apl-mrb
 * specific acpi info file named as apl-mrb_acpi_info.h
 * and put it in hypervisor/arch/x86/configs/apl-mrb/.
 */
#ifndef PLATFORM_ACPI_INFO_H
#define PLATFORM_ACPI_INFO_H

/* pm sstate data */
#define PM1A_EVT_ACCESS_SIZE	3U
#define PM1A_EVT_ADDRESS	0x400UL
#define PM1A_CNT_ADDRESS	0x404UL

#define WAKE_VECTOR_32		0x7A86BBDCUL
#define WAKE_VECTOR_64		0x7A86BBE8UL

/* reset register */
#define RESET_REGISTER_ADDRESS	0xCF9U
#define RESET_REGISTER_VALUE	0x0EU
#define RESET_REGISTER_SPACE_ID SPACE_SYSTEM_IO

/* DRHD of DMAR */
#define DRHD_COUNT		2U

#define DRHD0_DEV_CNT		1U
#define DRHD0_SEGMENT		0U
#define DRHD0_FLAGS		0U
#define DRHD0_REG_BASE		0xFED64000UL
#define DRHD0_IGNORE		true
#define DRHD0_DEVSCOPE0_BUS	0U
#define DRHD0_DEVSCOPE0_PATH	DEVFUN(0x2U, 0U)
#define DRHD0_DEVSCOPE1_BUS	0U
#define DRHD0_DEVSCOPE1_PATH	0U
#define DRHD0_DEVSCOPE2_BUS	0U
#define DRHD0_DEVSCOPE2_PATH	0U
#define DRHD0_DEVSCOPE3_BUS	0U
#define DRHD0_DEVSCOPE3_PATH	0U

#define DRHD1_DEV_CNT		1U
#define DRHD1_SEGMENT		0U
#define DRHD1_FLAGS		DRHD_FLAG_INCLUDE_PCI_ALL_MASK
#define DRHD1_REG_BASE		0xFED65000UL
#define DRHD1_IGNORE		false
#define DRHD1_DEVSCOPE0_BUS	0xFAU
#define DRHD1_DEVSCOPE0_PATH	0xF8U
#define DRHD1_DEVSCOPE1_BUS	0U
#define DRHD1_DEVSCOPE1_PATH	0U
#define DRHD1_DEVSCOPE2_BUS	0U
#define DRHD1_DEVSCOPE2_PATH	0U
#define DRHD1_DEVSCOPE3_BUS	0U
#define DRHD1_DEVSCOPE3_PATH	0U
#define DRHD1_IOAPIC_ID		8U

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
