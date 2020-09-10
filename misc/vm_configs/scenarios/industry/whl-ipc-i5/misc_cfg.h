/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define SOS_ROOTFS		"root=/dev/sda3 "
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
				"maxcpus=4"


#define SOS_VM_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(0U) | AFFINITY_CPU(1U) | AFFINITY_CPU(2U) | AFFINITY_CPU(3U))
#define VM1_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(0U) | AFFINITY_CPU(1U))
#define VM2_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(2U) | AFFINITY_CPU(3U))
#define VM3_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(0U) | AFFINITY_CPU(1U))
#define VM4_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(0U) | AFFINITY_CPU(1U))
#define VM5_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(0U) | AFFINITY_CPU(1U))
#define VM6_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(0U) | AFFINITY_CPU(1U))
#define VM7_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(0U) | AFFINITY_CPU(1U))

#ifdef CONFIG_RDT_ENABLED
#define HV_SUPPORTED_MAX_CLOS	0U
#define MAX_MBA_CLOS_NUM_ENTRIES	0U
#define MAX_CACHE_CLOS_NUM_ENTRIES	0U
#endif



#define VM0_PT_INTX_NUM	0U

#endif /* MISC_CFG_H */
