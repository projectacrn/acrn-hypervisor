/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define CONFIG_MAX_PCPU_NUM	4U
#define ROOTFS_0		"root=/dev/sda3 "
#define ROOTFS_1		"root=/dev/mmcblk0p1 "

#define SOS_ROOTFS		ROOTFS_0
#define SOS_CONSOLE		"console=ttyS0 "
#define SOS_COM1_BASE		0x3F8U
#define SOS_COM1_IRQ		4U
#define SOS_COM2_BASE		0x2F8U
#define SOS_COM2_IRQ		3U

#ifndef CONFIG_RELEASE
#define SOS_BOOTARGS_DIFF	"hvlog=2M@0x1FE00000 "	\
				"memmap=0x200000$0x1fe00000 "
#else
#define SOS_BOOTARGS_DIFF	""
#endif

#endif /* MISC_CFG_H */
