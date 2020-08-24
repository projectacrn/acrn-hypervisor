/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define SOS_ROOTFS		"root=/dev/mmcblk0p2 "
#define SOS_CONSOLE		"console=ttyS0 "
#define SOS_COM1_BASE		0x3F8U
#define SOS_COM1_IRQ		3U
#define SOS_COM2_BASE		0x2E8U
#define SOS_COM2_IRQ		4U

#define SOS_BOOTARGS_DIFF	"rw " \
				"rootwait "	\
				"console=tty0 "	\
				"consoleblank=0 "	\
				"no_timer_check "	\
				"quiet "	\
				"loglevel=3 "	\
				"i915.nuclear_pageflip=1"

#ifdef CONFIG_RDT_ENABLED
#define HV_SUPPORTED_MAX_CLOS		16U
#define MAX_MBA_CLOS_NUM_ENTRIES	16U
#define MAX_CACHE_CLOS_NUM_ENTRIES	16U

#define CLOS_MASK_0			0xfffU
#define CLOS_MASK_1			0xfffU
#define CLOS_MASK_2			0xfffU
#define CLOS_MASK_3			0xfffU
#define CLOS_MASK_4			0xfffU
#define CLOS_MASK_5			0xfffU
#define CLOS_MASK_6			0xfffU
#define CLOS_MASK_7			0xfffU
#define CLOS_MASK_8			0xfffU
#define CLOS_MASK_9			0xfffU
#define CLOS_MASK_10			0xfffU
#define CLOS_MASK_11			0xfffU
#define CLOS_MASK_12			0xfffU
#define CLOS_MASK_13			0xfffU
#define CLOS_MASK_14			0xfffU
#define CLOS_MASK_15			0xfffU

#define VM0_VCPU_CLOS			{0U}
#define VM1_VCPU_CLOS			{0U, 0U}
#define VM2_VCPU_CLOS			{0U, 0U}
#define VM3_VCPU_CLOS			{0U, 0U}
#define VM4_VCPU_CLOS			{0U, 0U}
#define VM5_VCPU_CLOS			{0U, 0U}
#define VM6_VCPU_CLOS			{0U, 0U}
#define VM7_VCPU_CLOS			{0U, 0U}
#endif

#endif /* MISC_CFG_H */
