/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define SERVICE_VM_ROOTFS "root=/dev/nvme0n1p2 "
#define SERVICE_VM_BOOTARGS_DIFF                                                                                       \
	"rw rootwait console=tty0 console=ttyS0 consoleblank=0 no_timer_check quiet loglevel=3 "                       \
	"i915.nuclear_pageflip=1 swiotlb=131072 maxcpus=4 hugepagesz=1G hugepages=5 "
#define SERVICE_VM_BOOTARGS_MISC "udmabuf.list_limit=8192 "
#define SERVICE_VM_CONFIG_CPU_AFFINITY (AFFINITY_CPU(0U) | AFFINITY_CPU(1U) | AFFINITY_CPU(2U) | AFFINITY_CPU(3U))
#define VM1_CONFIG_CPU_AFFINITY (AFFINITY_CPU(0U) | AFFINITY_CPU(1U))
#define VM2_CONFIG_CPU_AFFINITY (AFFINITY_CPU(2U) | AFFINITY_CPU(3U))
#ifdef CONFIG_RDT_ENABLED
#define HV_SUPPORTED_MAX_CLOS 0U
#define MAX_MBA_CLOS_NUM_ENTRIES 0U
#define MAX_CACHE_CLOS_NUM_ENTRIES 0U
#endif

#define PRE_RTVM_SW_SRAM_MAX_SIZE 0UL

#endif /* MISC_CFG_H */
