/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <vm_configurations.h>
#include <acrn_common.h>

extern struct acrn_vm_pci_ptdev_config vm0_pci_ptdevs[VM0_CONFIG_PCI_PTDEV_NUM];
extern struct acrn_vm_pci_ptdev_config vm1_pci_ptdevs[VM1_CONFIG_PCI_PTDEV_NUM];

struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{	/* VM0 */
		.type = PRE_LAUNCHED_VM,
		.name = "ACRN PRE-LAUNCHED VM0",
		.pcpu_bitmap = VM0_CONFIG_PCPU_BITMAP,
		.guest_flags = GUEST_FLAG_IO_COMPLETION_POLLING,
		.clos = 0U,
		.memory = {
			.start_hpa = VM0_CONFIG_MEM_START_HPA,
			.size = VM0_CONFIG_MEM_SIZE,
		},
		.os_config = {
			.name = "ClearLinux",
			.bootargs = VM0_CONFIG_OS_BOOTARG_CONSOLE	\
				VM0_CONFIG_OS_BOOTARG_MAXCPUS		\
				VM0_CONFIG_OS_BOOTARG_ROOT		\
				"rw rootwait noxsave nohpet console=hvc0 \
				no_timer_check ignore_loglevel log_buf_len=16M \
				consoleblank=0 tsc=reliable xapic_phys"
		},
		.vm_vuart = true,
		.pci_ptdev_num = VM0_CONFIG_PCI_PTDEV_NUM,
		.pci_ptdevs = vm0_pci_ptdevs,
	},
	{	/* VM1 */
		.type = PRE_LAUNCHED_VM,
		.name = "ACRN PRE-LAUNCHED VM1",
		.pcpu_bitmap = VM1_CONFIG_PCPU_BITMAP,
		.guest_flags = GUEST_FLAG_IO_COMPLETION_POLLING,
		.clos = 0U,
		.memory = {
			.start_hpa = VM1_CONFIG_MEM_START_HPA,
			.size = VM1_CONFIG_MEM_SIZE,
		},
		.os_config = {
			.name = "ClearLinux",
			.bootargs = VM1_CONFIG_OS_BOOTARG_CONSOLE	\
				VM1_CONFIG_OS_BOOTARG_MAXCPUS		\
				VM1_CONFIG_OS_BOOTARG_ROOT		\
				"rw rootwait noxsave nohpet console=hvc0 \
				no_timer_check ignore_loglevel log_buf_len=16M \
				consoleblank=0 tsc=reliable xapic_phys"
		},
		.vm_vuart = true,
		.pci_ptdev_num = VM1_CONFIG_PCI_PTDEV_NUM,
		.pci_ptdevs = vm1_pci_ptdevs,
	},
};
