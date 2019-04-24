/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H

/* Bits mask of guest flags that can be programmed by device model. Other bits are set by hypervisor only */
#define DM_OWNED_GUEST_FLAG_MASK	0UL

#define CONFIG_MAX_VM_NUM	2U

/* The VM CONFIGs like:
 *	VMX_CONFIG_PCPU_BITMAP
 *	VMX_CONFIG_MEM_START_HPA
 *	VMX_CONFIG_MEM_SIZE
 *	VMX_CONFIG_OS_BOOTARG_ROOT
 *	VMX_CONFIG_OS_BOOTARG_MAX_CPUS
 *	VMX_CONFIG_OS_BOOTARG_CONSOLE
 * might be different on your board, please modify them per your needs.
 */

#define VM0_CONFIG_PCPU_BITMAP			(PLUG_CPU(0) | PLUG_CPU(2))
#define VM0_CONFIG_NUM_CPUS			2U
#define VM0_CONFIG_MEM_START_HPA		0x100000000UL
#define VM0_CONFIG_MEM_SIZE			0x20000000UL
#define VM0_CONFIG_OS_BOOTARG_ROOT		"root=/dev/sda3 "
#define VM0_CONFIG_OS_BOOTARG_MAXCPUS		"maxcpus=2 "
#define VM0_CONFIG_OS_BOOTARG_CONSOLE		"console=ttyS0 "
#define VM0_CONFIG_PCI_PTDEV_NUM		3U

#define VM1_CONFIG_PCPU_BITMAP			(PLUG_CPU(1) | PLUG_CPU(3))
#define VM1_CONFIG_NUM_CPUS			2U
#define VM1_CONFIG_MEM_START_HPA		0x120000000UL
#define VM1_CONFIG_MEM_SIZE			0x20000000UL
#define VM1_CONFIG_OS_BOOTARG_ROOT		"root=/dev/sda3 "
#define VM1_CONFIG_OS_BOOTARG_MAXCPUS		"maxcpus=2 "
#define VM1_CONFIG_OS_BOOTARG_CONSOLE		"console=ttyS0 "
#define VM1_CONFIG_PCI_PTDEV_NUM		3U

#endif /* VM_CONFIGURATIONS_H */
