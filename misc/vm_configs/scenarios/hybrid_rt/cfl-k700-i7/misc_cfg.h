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
				"console=ttyS0,115200n8 "	\
				"ignore_loglevel "	\
				"no_timer_check "	\
				"hvlog=2M@0xe00000 "	\
				"memmap=0x200000$0xe00000 "	\
				"maxcpus=2"

#define VM0_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(2U) | AFFINITY_CPU(3U))

#define SOS_VM_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(0U) | AFFINITY_CPU(1U))
#define VM2_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(1U))

#ifdef CONFIG_RDT_ENABLED

/*
 * The maximum CLOS that is allowed by ACRN hypervisor,
 * its value is set to be least common Max CLOS (CPUID.(EAX=0x10,ECX=ResID):EDX[15:0])
 * among all supported RDT resources in the platform. In other words, it is
 * min(maximum CLOS of L2, L3 and MBA). This is done in order to have consistent
 * CLOS allocations between all the RDT resources.
 */
#define HV_SUPPORTED_MAX_CLOS	0U

/*
 * Max number of Cache Mask entries corresponding to each CLOS.
 * This can vary if CDP is enabled vs disabled, as each CLOS entry
 * will have corresponding cache mask values for Data and Code when
 * CDP is enabled.
 */
#define MAX_MBA_CLOS_NUM_ENTRIES	0U

/* Max number of MBA delay entries corresponding to each CLOS. */
#define MAX_CACHE_CLOS_NUM_ENTRIES	0U
#endif

#define VM0_CONFIG_PCI_DEV_NUM	4U
#define VM2_CONFIG_PCI_DEV_NUM	1U

#define VM0_BOOT_ARGS	"rw rootwait root=/dev/nvme0n1p2 earlyprintk=serial,ttyS0,115200 \
console=ttyS0,115200n8 log_buf_len=2M ignore_loglevel noxsave \
nohpet no_timer_check tsc=reliable"


#define VM0_PT_INTX_NUM	0U

#endif /* MISC_CFG_H */
