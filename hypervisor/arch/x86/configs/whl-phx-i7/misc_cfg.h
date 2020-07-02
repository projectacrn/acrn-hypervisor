/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef MISC_CFG_H
#define MISC_CFG_H

#define MAX_PCPU_NUM	4U
#define MAX_PLATFORM_CLOS_NUM	0U
#define ROOTFS_0		"root=/dev/nvme0n1p3 "
#define ROOTFS_1		"root=/dev/sda3 "

#define SOS_ROOTFS		"root=/dev/sda3 "
#define SOS_CONSOLE		"console=ttyS0 "
#define SOS_COM1_BASE		0x3F8U
#define SOS_COM1_IRQ		5U
#define SOS_COM2_BASE		0x2F8U
#define SOS_COM2_IRQ		5U

#define SOS_BOOTARGS_DIFF	"rw " \
				"rootwait "	\
				"console=tty0 "	\
				"consoleblank=0 "	\
				"no_timer_check "	\
				"quiet "	\
				"loglevel=3 "	\
				"i915.nuclear_pageflip=1 "	\
				"hvlog=2M@0xe00000 "	\
				"memmap=0x200000$0xe00000"

#define MAX_HIDDEN_PDEVS_NUM	0U

#define HI_MMIO_START		~0UL
#define HI_MMIO_END		0UL

#endif /* MISC_CFG_H */
