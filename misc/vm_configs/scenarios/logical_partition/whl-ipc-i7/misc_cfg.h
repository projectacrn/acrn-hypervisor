/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define VM0_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(0U) | AFFINITY_CPU(2U))
#define VM1_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(1U) | AFFINITY_CPU(3U))

#ifdef CONFIG_RDT_ENABLED
#define HV_SUPPORTED_MAX_CLOS	0U
#define MAX_MBA_CLOS_NUM_ENTRIES	0U
#define MAX_CACHE_CLOS_NUM_ENTRIES	0U
#endif

#define VM0_PASSTHROUGH_TPM
#define VM0_TPM_BUFFER_BASE_ADDR   0xFED40000UL
#define VM0_TPM_BUFFER_BASE_ADDR_GPA   0xFED40000UL
#define VM0_TPM_BUFFER_SIZE        0x5000UL

#define VM0_CONFIG_PCI_DEV_NUM	3U
#define VM1_CONFIG_PCI_DEV_NUM	3U

#define VM0_BOOT_ARGS	"rw rootwait root=/dev/sda3 console=ttyS0 \
noxsave nohpet no_timer_check ignore_loglevel \
log_buf_len=16M consoleblank=0 tsc=reliable"

#define VM1_BOOT_ARGS	"rw rootwait root=/dev/sda3 console=ttyS0 \
noxsave nohpet no_timer_check ignore_loglevel \
log_buf_len=16M consoleblank=0 tsc=reliable"


#define VM0_PT_INTX_NUM	0U

#endif /* MISC_CFG_H */
