/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define ROOTFS_0		"root=/dev/sda3 "

#define SOS_ROOTFS		ROOTFS_0
#define SOS_CONSOLE		"console=ttyS0 "
#define SOS_COM1_BASE		0x3F8U
#define SOS_COM1_IRQ		4U

#ifndef CONFIG_RELEASE
#define SOS_BOOTARGS_DIFF	"hvlog=2M@0x1FE00000"
#else
#define SOS_BOOTARGS_DIFF	""
#endif

#endif /* MISC_CFG_H */
