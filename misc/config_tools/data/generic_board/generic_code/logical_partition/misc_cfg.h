/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define VM0_CONFIG_CPU_AFFINITY (AFFINITY_CPU(0U) | AFFINITY_CPU(2U))
#define VM1_CONFIG_CPU_AFFINITY (AFFINITY_CPU(1U) | AFFINITY_CPU(3U))
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
#endif

#define VM0_BOOT_ARGS                                                                                                  \
	"rw rootwait root=/dev/sda3 console=ttyS0 noxsave nohpet no_timer_check ignore_loglevel log_buf_len=16M "      \
	"consoleblank=0 tsc=reliable reboot=acpi"
#define VM1_BOOT_ARGS                                                                                                  \
	"rw rootwait root=/dev/sda3 console=ttyS0 noxsave nohpet no_timer_check ignore_loglevel log_buf_len=16M "      \
	"consoleblank=0 tsc=reliable reboot=acpi"
#define VM0_PT_INTX_NUM 0U

#endif /* MISC_CFG_H */
