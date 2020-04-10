/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define MAX_PCPU_NUM	4U
#define MAX_PLATFORM_CLOS_NUM	4U

#define ROOTFS_0		"root=/dev/sda3 "
#define ROOTFS_1		"root=/dev/mmcblk0p3 "

#define SOS_ROOTFS		ROOTFS_1
#define SOS_CONSOLE		"console=ttyS0 "
#define SOS_COM1_BASE		0x3F8U
#define SOS_COM1_IRQ		4U
#define SOS_COM2_BASE		0x2F8U
#define SOS_COM2_IRQ		3U

#ifndef CONFIG_RELEASE
#define BOOTARG_DEBUG		"hvlog=2M@0x5de00000 "	\
				"memmap=0x200000$0x5de00000 "	\
				"memmap=0x400000$0x5da00000 "	\
				"ramoops.mem_address=0x5da00000 "	\
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

#define MAX_HIDDEN_PDEVS_NUM	1U

#define HI_MMIO_START		~0UL
#define HI_MMIO_END		0UL


#endif /* MISC_CFG_H */
