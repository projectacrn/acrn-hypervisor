/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef MISC_CFG_H
#define MISC_CFG_H

#define MAX_PCPU_NUM		4U
#define MAX_PLATFORM_CLOS_NUM	0U
#define MAX_VMSIX_ON_MSI_PDEVS_NUM	0U

#define ROOTFS_0		"root=/dev/nvme0n1p3 "
#define ROOTFS_1		"root=/dev/sda3 "

#define SOS_ROOTFS		ROOTFS_1
#define SOS_CONSOLE		"console=ttyS0 "
#define SOS_COM1_BASE		0x3F8U
#define SOS_COM1_IRQ		4U
#define SOS_COM2_BASE		0x2F8U
#define SOS_COM2_IRQ		3U

#ifndef CONFIG_RELEASE
#define SOS_BOOTARGS_DIFF	"hvlog=2M@0xE00000 memmap=0x200000$0xE00000 "
#else
#define SOS_BOOTARGS_DIFF	""
#endif

#define MAX_HIDDEN_PDEVS_NUM	0U

#define HI_MMIO_START		~0UL
#define HI_MMIO_END		0UL

#define VM0_PASSTHROUGH_TPM
#define VM0_TPM_BUFFER_BASE_ADDR    0xFED40000UL
#define VM0_TPM_BUFFER_SIZE         0x5000UL

#endif /* MISC_CFG_H */
