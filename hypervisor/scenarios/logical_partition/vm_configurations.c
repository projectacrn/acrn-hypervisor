/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <vuart.h>

extern struct acrn_vm_pci_dev_config vm0_pci_devs[VM0_CONFIG_PCI_DEV_NUM];
extern struct acrn_vm_pci_dev_config vm1_pci_devs[VM1_CONFIG_PCI_DEV_NUM];

struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{	/* VM0 */
		CONFIG_PRE_STD_VM(1),
		.name = "ACRN PRE-LAUNCHED VM0",
		.cpu_affinity = VM0_CONFIG_CPU_AFFINITY,
		.memory = {
			.start_hpa = VM0_CONFIG_MEM_START_HPA,
			.size = VM0_CONFIG_MEM_SIZE,
		},
		.os_config = {
			.name = "ClearLinux",
			.kernel_type = KERNEL_BZIMAGE,
			.kernel_mod_tag = "Linux_bzImage",
			.bootargs = VM0_CONFIG_OS_BOOTARG_CONSOLE	\
				VM0_CONFIG_OS_BOOTARG_MAXCPUS		\
				VM0_CONFIG_OS_BOOTARG_ROOT		\
				"rw rootwait noxsave nohpet \
				no_timer_check ignore_loglevel log_buf_len=16M \
				consoleblank=0 tsc=reliable"
		},
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM2_BASE,
			.irq = COM2_IRQ,
			.t_vuart.vm_id = 1U,
			.t_vuart.vuart_id = 1U,
		},
		.pci_dev_num = VM0_CONFIG_PCI_DEV_NUM,
		.pci_devs = vm0_pci_devs,
	},
	{	/* VM1 */
		CONFIG_PRE_STD_VM(2),
		.name = "ACRN PRE-LAUNCHED VM1",
		.cpu_affinity = VM1_CONFIG_CPU_AFFINITY,
		.guest_flags = (GUEST_FLAG_RT | GUEST_FLAG_LAPIC_PASSTHROUGH),
		.memory = {
			.start_hpa = VM1_CONFIG_MEM_START_HPA,
			.size = VM1_CONFIG_MEM_SIZE,
		},
		.os_config = {
			.name = "ClearLinux",
			.kernel_type = KERNEL_BZIMAGE,
			.kernel_mod_tag = "Linux_bzImage",
			.bootargs = VM1_CONFIG_OS_BOOTARG_CONSOLE	\
				VM1_CONFIG_OS_BOOTARG_MAXCPUS		\
				VM1_CONFIG_OS_BOOTARG_ROOT		\
				"rw rootwait noxsave nohpet \
				no_timer_check ignore_loglevel log_buf_len=16M \
				consoleblank=0 tsc=reliable"
		},
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM2_BASE,
			.irq = COM2_IRQ,
			.t_vuart.vm_id = 0U,
			.t_vuart.vuart_id = 1U,
		},
		.pci_dev_num = VM1_CONFIG_PCI_DEV_NUM,
		.pci_devs = vm1_pci_devs,
	},
};
