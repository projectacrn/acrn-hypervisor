/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H

#include <pci_devices.h>

/* The VM CONFIGs like:
 *	VMX_CONFIG_PCPU_BITMAP
 *	VMX_CONFIG_MEM_START_HPA
 *	VMX_CONFIG_MEM_SIZE
 *	VMX_CONFIG_OS_BOOTARG_ROOT
 *	VMX_CONFIG_OS_BOOTARG_MAX_CPUS
 *	VMX_CONFIG_OS_BOOTARG_CONSOLE
 * might be different on your board, please modify them per your needs.
 */

#define	VM0_CONFIGURED

#define VM0_CONFIG_NAME				"ACRN PRE-LAUNCHED VM0"
#define VM0_CONFIG_TYPE				PRE_LAUNCHED_VM
#define VM0_CONFIG_PCPU_BITMAP			(PLUG_CPU(0) | PLUG_CPU(2))
#define VM0_CONFIG_FLAGS			GUEST_FLAG_IO_COMPLETION_POLLING
#define VM0_CONFIG_MEM_START_HPA		0x100000000UL
#define VM0_CONFIG_MEM_SIZE			0x20000000UL

#define VM0_CONFIG_OS_NAME			"ClearLinux"
#define VM0_CONFIG_OS_BOOTARG_ROOT		"root=/dev/sda3 "
#define VM0_CONFIG_OS_BOOTARG_MAX_CPUS		"max_cpus=2 "
#define VM0_CONFIG_OS_BOOTARG_CONSOLE		"console=ttyS2 "
#define VM0_CONFIG_OS_BOOTARGS			VM0_CONFIG_OS_BOOTARG_CONSOLE	\
						VM0_CONFIG_OS_BOOTARG_ROOT	\
						VM0_CONFIG_OS_BOOTARG_MAX_CPUS	\
						"rw rootwait noxsave nohpet console=hvc0 \
						no_timer_check ignore_loglevel log_buf_len=16M \
						consoleblank=0 tsc=reliable xapic_phys"

#define	VM1_CONFIGURED

#define VM1_CONFIG_NAME				"ACRN PRE-LAUNCHED VM1"
#define VM1_CONFIG_TYPE				PRE_LAUNCHED_VM
#define VM1_CONFIG_PCPU_BITMAP			(PLUG_CPU(1) | PLUG_CPU(3))
#define VM1_CONFIG_FLAGS			GUEST_FLAG_IO_COMPLETION_POLLING
#define VM1_CONFIG_MEM_START_HPA		0x120000000UL
#define VM1_CONFIG_MEM_SIZE			0x20000000UL

#define VM1_CONFIG_OS_NAME			"ClearLinux"
#define VM1_CONFIG_OS_BOOTARG_ROOT		"root=/dev/sda3 "
#define VM1_CONFIG_OS_BOOTARG_MAX_CPUS		"max_cpus=2 "
#define VM1_CONFIG_OS_BOOTARG_CONSOLE		"console=ttyS2 "
#define VM1_CONFIG_OS_BOOTARGS			VM1_CONFIG_OS_BOOTARG_CONSOLE	\
						VM1_CONFIG_OS_BOOTARG_ROOT	\
						VM1_CONFIG_OS_BOOTARG_MAX_CPUS	\
						"rw rootwait noxsave nohpet console=hvc0 \
						no_timer_check ignore_loglevel log_buf_len=16M \
						consoleblank=0 tsc=reliable xapic_phys"


#define VM0_CONFIG_PCI_PTDEV_NUM		3U
extern struct acrn_vm_pci_ptdev_config vm0_pci_ptdevs[VM0_CONFIG_PCI_PTDEV_NUM];

#define VM1_CONFIG_PCI_PTDEV_NUM		3U
extern struct acrn_vm_pci_ptdev_config vm1_pci_ptdevs[VM0_CONFIG_PCI_PTDEV_NUM];

#endif /* VM_CONFIGURATIONS_H */
