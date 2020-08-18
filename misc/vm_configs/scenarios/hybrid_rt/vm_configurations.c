/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <vm_config.h>
#include <vuart.h>
#include <pci_dev.h>

extern struct acrn_vm_pci_dev_config vm0_pci_devs[VM0_CONFIG_PCI_DEV_NUM];
extern struct acrn_vm_pci_dev_config vm2_pci_devs[VM2_CONFIG_PCI_DEV_NUM];

struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{	/* VM0 */
		CONFIG_PRE_RT_VM(1),
		.name = "ACRN PRE-LAUNCHED VM0",
		.cpu_affinity = VM0_CONFIG_CPU_AFFINITY,
		.guest_flags = (GUEST_FLAG_LAPIC_PASSTHROUGH | GUEST_FLAG_RT),
		.memory = {
			.start_hpa = VM0_CONFIG_MEM_START_HPA,
			.size = VM0_CONFIG_MEM_SIZE,
			.start_hpa2 = VM0_CONFIG_MEM_START_HPA2,
			.size_hpa2 = VM0_CONFIG_MEM_SIZE_HPA2,
		},
		.os_config = {
			.name = "PREEMPT-RT",
			.kernel_type = KERNEL_BZIMAGE,
			.kernel_mod_tag = "RT_bzImage",
			.bootargs = "rw rootwait root=/dev/sda3 console=ttyS0 \
				noxsave nohpet no_timer_check ignore_loglevel \
				consoleblank=0 tsc=reliable "
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
#ifdef VM0_PASSTHROUGH_TPM
		.pt_tpm2 = true,
		.mmiodevs[0] = {
			.base_gpa = 0xFED40000UL,
			.base_hpa = VM0_TPM_BUFFER_BASE_ADDR,
			.size = VM0_TPM_BUFFER_SIZE,
		},
#endif
	},
	{	/* VM1 */
		CONFIG_SOS_VM,
		.name = "ACRN SOS VM",

		/* Allow SOS to reboot the host since there is supposed to be the highest severity guest */
		.guest_flags = 0UL,
		.cpu_affinity = SOS_VM_CONFIG_CPU_AFFINITY,
		.memory = {
			.start_hpa = 0UL,
			.size = CONFIG_SOS_RAM_SIZE,
		},
		.os_config = {
			.name = "ACRN Service OS",
			.kernel_type = KERNEL_BZIMAGE,
			.kernel_mod_tag = "Linux_bzImage",
			.bootargs = SOS_VM_BOOTARGS,
		},
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = SOS_COM1_BASE,
			.irq = SOS_COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = SOS_COM2_BASE,
			.irq = SOS_COM2_IRQ,
			.t_vuart.vm_id = 0U,
			.t_vuart.vuart_id = 1U,
		},
	},
	{	/* VM2 */
		CONFIG_POST_STD_VM(1),
		/* The PCI device configuration is only for in-hypervisor vPCI devices. */
		.pci_dev_num = VM2_CONFIG_PCI_DEV_NUM,
		.pci_devs = vm2_pci_devs,
		.cpu_affinity = VM2_CONFIG_CPU_AFFINITY,
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
	},
};
