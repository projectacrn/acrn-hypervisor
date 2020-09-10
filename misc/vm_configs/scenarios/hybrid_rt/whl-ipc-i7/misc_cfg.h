/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define SOS_ROOTFS		"root=/dev/nvme0n1p3 "
#define SOS_CONSOLE		"console=ttyS0 "
#define SOS_COM1_BASE		0x3F8U
#define SOS_COM1_IRQ		4U
#define SOS_COM2_BASE		0x2F8U
#define SOS_COM2_IRQ		3U

#define SOS_BOOTARGS_DIFF	"rw " \
				"rootwait "	\
				"console=tty0 "	\
				"consoleblank=0 "	\
				"no_timer_check "	\
				"quiet "	\
				"loglevel=3 "	\
				"i915.nuclear_pageflip=1 "	\
				"hvlog=2M@0xe00000 "	\
				"memmap=0x200000$0xe00000 "	\
				"maxcpus=2"

#define VM0_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(2U) | AFFINITY_CPU(3U))

#define SOS_VM_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(0U) | AFFINITY_CPU(1U))
#define VM2_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(1U))

#ifdef CONFIG_RDT_ENABLED
#define HV_SUPPORTED_MAX_CLOS	0U
#define MAX_MBA_CLOS_NUM_ENTRIES	0U
#define MAX_CACHE_CLOS_NUM_ENTRIES	0U
#endif

#define VM0_PASSTHROUGH_TPM
#define VM0_TPM_BUFFER_BASE_ADDR   0xFED40000UL
#define VM0_TPM_BUFFER_BASE_ADDR_GPA   0xFED40000UL
#define VM0_TPM_BUFFER_SIZE        0x5000UL

#define VM0_CONFIG_PCI_DEV_NUM	4U
#define VM2_CONFIG_PCI_DEV_NUM	1U

#define VM0_BOOT_ARGS	"rw rootwait root=/dev/sda3 console=ttyS0 \
noxsave nohpet no_timer_check ignore_loglevel \
consoleblank=0 tsc=reliable"


#define VM0_PT_INTX_NUM	0U

#endif /* MISC_CFG_H */
