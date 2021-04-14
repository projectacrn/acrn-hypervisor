/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define SOS_ROOTFS "root=/dev/nvme0n1p3"
#define SOS_CONSOLE "console=ttyS0"
#define SOS_COM1_BASE 0x3F8U
#define SOS_COM1_IRQ 4U
#define SOS_COM2_BASE 0x2F8U
#define SOS_COM2_IRQ 5U

#define SOS_BOOTARGS_DIFF                                                                                              \
	"rw rootwait console=tty0 consoleblank=0 no_timer_check quiet loglevel=3 i915.nuclear_pageflip=1 "             \
	"swiotlb=131072 maxcpus=2"
#define VM0_CONFIG_CPU_AFFINITY (AFFINITY_CPU(2U) | AFFINITY_CPU(3U))
#define SOS_VM_CONFIG_CPU_AFFINITY (AFFINITY_CPU(0U) | AFFINITY_CPU(1U))
#define VM2_CONFIG_CPU_AFFINITY (AFFINITY_CPU(0U) | AFFINITY_CPU(1U))
#define VM3_CONFIG_CPU_AFFINITY (AFFINITY_CPU(1U))
#ifdef CONFIG_RDT_ENABLED
#define HV_SUPPORTED_MAX_CLOS 0U
#define MAX_MBA_CLOS_NUM_ENTRIES 0U
#define MAX_CACHE_CLOS_NUM_ENTRIES 0U
#define MBA_MASK_0 0U
#define CLOS_MASK_0 0xfffU
#define CLOS_MASK_1 0xfffU
#define CLOS_MASK_2 0xfffU
#define CLOS_MASK_3 0xfffU
#define CLOS_MASK_4 0xfffU
#define CLOS_MASK_5 0xfffU
#define CLOS_MASK_6 0xfffU
#define CLOS_MASK_7 0xfffU
#define CLOS_MASK_8 0xfffU
#define CLOS_MASK_9 0xfffU
#define CLOS_MASK_10 0xfffU
#define CLOS_MASK_11 0xfffU
#define CLOS_MASK_12 0xfffU
#define CLOS_MASK_13 0xfffU
#define CLOS_MASK_14 0xfffU
#define CLOS_MASK_15 0xfffU
#define VM0_VCPU_CLOS                                                                                                  \
	{ 0U, 0U }
#define VM1_VCPU_CLOS                                                                                                  \
	{ 0U, 0U }
#define VM2_VCPU_CLOS                                                                                                  \
	{ 0U, 0U }
#define VM3_VCPU_CLOS                                                                                                  \
	{ 0U }
#endif
#define VM0_CONFIG_PCI_DEV_NUM 4U
#define VM2_CONFIG_PCI_DEV_NUM 1U

#define VM0_BOOT_ARGS                                                                                                  \
	"rw rootwait root=/dev/sda3 no_ipi_broadcast=1 console=ttyS0 noxsave nohpet no_timer_check ignore_loglevel "   \
	"consoleblank=0 tsc=reliable clocksource=tsc x2apic_phys processor.max_cstate=0 intel_idle.max_cstate=0 "      \
	"intel_pstate=disable mce=ignore_ce audit=0 isolcpus=nohz,domain,1 nohz_full=1 rcu_nocbs=1 nosoftlockup "      \
	"idle=poll irqaffinity=0 reboot=acpi"
#define VM0_PT_INTX_NUM 0U

#endif /* MISC_CFG_H */
