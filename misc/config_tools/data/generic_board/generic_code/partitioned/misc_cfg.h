/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define VM0_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(0U) | AFFINITY_CPU(2U))
#define VM1_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(1U) | AFFINITY_CPU(3U))

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

#define CLOS_MASK_0			0xfffffU
#define CLOS_MASK_1			0xfffffU
#define CLOS_MASK_2			0xfffffU
#define CLOS_MASK_3			0xfffffU
#define CLOS_MASK_4			0xfffffU
#define CLOS_MASK_5			0xfffffU
#define CLOS_MASK_6			0xfffffU
#define CLOS_MASK_7			0xfffffU

#define VM0_VCPU_CLOS			{0U, 0U}
#define VM1_VCPU_CLOS			{0U, 0U}
#endif

#define VM0_CONFIG_PCI_DEV_NUM	3U
#define VM1_CONFIG_PCI_DEV_NUM	2U

#define VM0_BOOT_ARGS	"rw rootwait root=/dev/sda3 console=ttyS0 \
noxsave nohpet no_timer_check ignore_loglevel \
log_buf_len=16M consoleblank=0 tsc=reliable reboot=acpi"

#define VM1_BOOT_ARGS	"rw rootwait root=/dev/sda2 console=ttyS0 \
noxsave nohpet no_timer_check ignore_loglevel \
log_buf_len=16M consoleblank=0 tsc=reliable reboot=acpi"


#define VM0_PT_INTX_NUM	0U

#endif /* MISC_CFG_H */
