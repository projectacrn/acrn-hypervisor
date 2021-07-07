/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/vm_config.h>
#include <vuart.h>
#include <asm/pci_dev.h>
extern struct acrn_vm_pci_dev_config sos_pci_devs[CONFIG_MAX_PCI_DEV_NUM];
extern struct pt_intx_config vm0_pt_intx[1U];
struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{
		/* VM0 */
		CONFIG_SAFETY_VM(1),
		.name = "ACRN PRE-LAUNCHED VM0",
		.guest_flags = 0UL,
#ifdef CONFIG_RDT_ENABLED
		.clos = VM0_VCPU_CLOS,
#endif
		.cpu_affinity = VM0_CONFIG_CPU_AFFINITY,
		.memory =
			{
				.start_hpa = VM0_CONFIG_MEM_START_HPA,
				.size = VM0_CONFIG_MEM_SIZE,
				.start_hpa2 = VM0_CONFIG_MEM_START_HPA2,
				.size_hpa2 = VM0_CONFIG_MEM_SIZE_HPA2,
			},
		.os_config =
			{
				.name = "Zephyr",
				.kernel_type = KERNEL_ZEPHYR,
				.kernel_mod_tag = "Zephyr_RawImage",
				.kernel_load_addr = 0x8000,
				.kernel_entry_addr = 0x8000,
			},
		.acpi_config =
			{
				.acpi_mod_tag = "ACPI_VM0",
			},
		.vuart[0] =
			{
				.type = VUART_LEGACY_PIO,
				.addr.port_base = COM1_BASE,
				.irq = COM1_IRQ,
			},
		.vuart[1] =
			{
				.type = VUART_LEGACY_PIO,
				.addr.port_base = COM2_BASE,
				.irq = COM2_IRQ,
				.t_vuart.vm_id = 1U,
				.t_vuart.vuart_id = 1U,
			},
#ifdef VM0_PASSTHROUGH_TPM
		.pt_tpm2 = true,
		.mmiodevs[0] =
			{
				.user_vm_pa = VM0_TPM_BUFFER_BASE_ADDR_GPA,
				.service_vm_pa = VM0_TPM_BUFFER_BASE_ADDR,
				.size = VM0_TPM_BUFFER_SIZE,
			},
#endif
#ifdef P2SB_BAR_ADDR
		.pt_p2sb_bar = true,
		.mmiodevs[0] =
			{
				.user_vm_pa = P2SB_BAR_ADDR_GPA,
				.service_vm_pa = P2SB_BAR_ADDR,
				.size = P2SB_BAR_SIZE,
			},
#endif
		.pt_intx_num = VM0_PT_INTX_NUM,
		.pt_intx = &vm0_pt_intx[0U],
	},
	{
		/* VM1 */
		CONFIG_SOS_VM,
		.name = "ACRN SOS VM",
		/* Allow Service VM to reboot the system since it is the highest priority VM. */
		.guest_flags = 0UL,
#ifdef CONFIG_RDT_ENABLED
		.clos = VM1_VCPU_CLOS,
#endif
		.cpu_affinity = SOS_VM_CONFIG_CPU_AFFINITY,
		.memory =
			{
				.start_hpa = 0UL,
			},
		.os_config =
			{
				.name = "ACRN Service OS",
				.kernel_type = KERNEL_BZIMAGE,
				.kernel_mod_tag = "Linux_bzImage",
				.bootargs = SOS_VM_BOOTARGS,
			},
		.vuart[0] =
			{
				.type = VUART_LEGACY_PIO,
				.addr.port_base = SOS_COM1_BASE,
				.irq = SOS_COM1_IRQ,
			},
		.vuart[1] =
			{
				.type = VUART_LEGACY_PIO,
				.addr.port_base = SOS_COM2_BASE,
				.irq = SOS_COM2_IRQ,
				.t_vuart.vm_id = 0U,
				.t_vuart.vuart_id = 1U,
			},
		.pci_dev_num = 0U,
		.pci_devs = sos_pci_devs,
	},
	{
		/* VM2 */
		CONFIG_POST_STD_VM(1),
#ifdef CONFIG_RDT_ENABLED
		.clos = VM2_VCPU_CLOS,
#endif
		.cpu_affinity = VM2_CONFIG_CPU_AFFINITY,
		.vuart[0] =
			{
				.type = VUART_LEGACY_PIO,
				.addr.port_base = COM1_BASE,
				.irq = COM1_IRQ,
			},
		.vuart[1] =
			{
				.type = VUART_LEGACY_PIO,
				.addr.port_base = INVALID_COM_BASE,
			},
	},
};
