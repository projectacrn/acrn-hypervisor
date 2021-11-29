/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define SERVICE_VM_ROOTFS             "root=/dev/nvme0n1p3 "
#define SERVICE_VM_OS_CONSOLE         "console=ttyS0 "
#define SERVICE_VM_BOOTARGS_DIFF      "rw rootwait console=tty0 consoleblank=0 no_timer_check quiet loglevel=3 i915.nuclear_pageflip=1 swiotlb=131072 maxcpus=3 hugepagesz=1G hugepages=8 "
#define VM0_CONFIG_CPU_AFFINITY       (AFFINITY_CPU(3U))
#define SERVICE_VM_CONFIG_CPU_AFFINITY (AFFINITY_CPU(0U)|AFFINITY_CPU(1U)|AFFINITY_CPU(2U))
#define VM2_CONFIG_CPU_AFFINITY       (AFFINITY_CPU(2U))
#define VM3_CONFIG_CPU_AFFINITY       (AFFINITY_CPU(2U))
#ifdef CONFIG_RDT_ENABLED
#define HV_SUPPORTED_MAX_CLOS         0U
#define MAX_MBA_CLOS_NUM_ENTRIES      0U
#define MAX_CACHE_CLOS_NUM_ENTRIES    0U
#define CLOS_MASK_0                   0xfffffU
#define CLOS_MASK_1                   0xfffffU
#define CLOS_MASK_2                   0xfffffU
#define CLOS_MASK_3                   0xfffffU
#define CLOS_MASK_4                   0xfffffU
#define CLOS_MASK_5                   0xfffffU
#define CLOS_MASK_6                   0xfffffU
#define CLOS_MASK_7                   0xfffffU
#endif


#endif /* MISC_CFG_H */
