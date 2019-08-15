/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define ROOTFS_0		"root=/dev/sda3 "
#define ROOTFS_1		"root=/dev/mmcblk1p1 "

#define SOS_ROOTFS		ROOTFS_1
#define SOS_CONSOLE		"console=ttyS2 "
#define SOS_COM1_BASE		0x3E8U
#define SOS_COM1_IRQ		6U
#define SOS_COM2_BASE		0x3F8U
#define SOS_COM2_IRQ		10U

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
				"i915.enable_initial_modeset=1 "	\
				"i915.enable_guc=0x02 "	\
				"video=DP-1:d "	\
				"video=DP-2:d "	\
				"cma=64M@0- "	\
				"panic_print=0x1f"

#endif /* MISC_CFG_H */
