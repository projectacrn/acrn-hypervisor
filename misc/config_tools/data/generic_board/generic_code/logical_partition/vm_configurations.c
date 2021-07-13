/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/vm_config.h>
#include <vuart.h>
#include <asm/pci_dev.h>
extern struct pt_intx_config vm0_pt_intx[1U];
struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{
		/* VM0 */
		CONFIG_PRE_STD_VM(1),
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
				.name = "YOCTO",
				.kernel_type = KERNEL_BZIMAGE,
				.kernel_mod_tag = "Linux_bzImage",
				.bootargs = VM0_BOOT_ARGS,
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
		CONFIG_PRE_STD_VM(2),
		.name = "ACRN PRE-LAUNCHED VM1",
		.guest_flags = 0UL,
#ifdef CONFIG_RDT_ENABLED
		.clos = VM1_VCPU_CLOS,
#endif
		.cpu_affinity = VM1_CONFIG_CPU_AFFINITY,
		.memory =
			{
				.start_hpa = VM1_CONFIG_MEM_START_HPA,
				.size = VM1_CONFIG_MEM_SIZE,
				.start_hpa2 = VM1_CONFIG_MEM_START_HPA2,
				.size_hpa2 = VM1_CONFIG_MEM_SIZE_HPA2,
			},
		.os_config =
			{
				.name = "YOCTO",
				.kernel_type = KERNEL_BZIMAGE,
				.kernel_mod_tag = "Linux_bzImage",
				.bootargs = VM1_BOOT_ARGS,
			},
		.acpi_config =
			{
				.acpi_mod_tag = "ACPI_VM1",
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
				.t_vuart.vm_id = 0U,
				.t_vuart.vuart_id = 1U,
			},
	},
};
