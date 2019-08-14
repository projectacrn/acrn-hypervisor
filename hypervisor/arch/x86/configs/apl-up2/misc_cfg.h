/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define ROOTFS_0		"root=/dev/sda3 "
#define ROOTFS_1		"root=/dev/mmcblk0p1 "

#define SOS_ROOTFS		ROOTFS_1
#define SOS_CONSOLE		"console=ttyS0 "
#define SOS_COM1_BASE		0x3F8U
#define SOS_COM1_IRQ		4U

#ifndef CONFIG_RELEASE
#define BOOTARG_DEBUG		"hvlog=2M@0x6de00000 "	\
				"memmap=0x400000$0x6da00000 "	\
				"ramoops.mem_address=0x6da00000 "	\
				"ramoops.mem_size=0x400000 "	\
				"ramoops.console_size=0x200000 "	\
				"reboot_panic=p,w "
#else
#define BOOTARG_DEBUG		""
#endif

#define SOS_BOOTARGS_DIFF	BOOTARG_DEBUG	\
				"module_blacklist=dwc3_pci "	\
				"i915.enable_guc=0x02 "	\
				"cma=64M@0- "

#endif /* MISC_CFG_H */
