/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/vm_config.h>
#include <vuart.h>
#include <asm/pci_dev.h>
#include <asm/pgtable.h>
#include <schedule.h>

extern struct acrn_vm_pci_dev_config vm0_pci_devs[VM0_CONFIG_PCI_DEV_NUM];
extern struct pt_intx_config vm0_pt_intx[1U];
static struct vm_hpa_regions vm0_hpa[] = {
	{.start_hpa = 0x100000000, .size_hpa = 0x20000000},
};
extern struct acrn_vm_pci_dev_config vm1_pci_devs[VM1_CONFIG_PCI_DEV_NUM];
extern struct pt_intx_config vm1_pt_intx[1U];
static struct vm_hpa_regions vm1_hpa[] = {
	{.start_hpa = 0x120000000, .size_hpa = 0x20000000},
};
struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{
		/* Static configured VM0 */
		CONFIG_PRE_STD_VM,
		.name = "PRE_STD_VM0",
		.vm_prio = PRIO_LOW,
		.companion_vm_id = 65535U,
		.guest_flags = (GUEST_FLAG_STATIC_VM),
		.cpu_affinity = VM0_CONFIG_CPU_AFFINITY,
		.memory =
			{
				.region_num = 1,
				.host_regions = vm0_hpa,
			},
		.os_config =
			{
				.name = "",
				.kernel_type = KERNEL_BZIMAGE,
				.kernel_mod_tag = "Linux_bzImage",
				.ramdisk_mod_tag = "Ubuntu",
				.bootargs = VM0_BOOT_ARGS,
			},
		.acpi_config =
			{
				.acpi_mod_tag = "ACPI_VM0",
			},
		.vuart[0] =
			{
				.type = VUART_LEGACY_PIO,
				.addr.port_base = 0x3F8U,
				.irq = 4U,
			},
		.vuart[1] =
			{
				.irq = 3U,
				.type = VUART_LEGACY_PIO,
				.addr.port_base = 0x2F8U,
				.t_vuart.vm_id = 1U,
				.t_vuart.vuart_id = 1U,
			},
		.pci_dev_num = VM0_CONFIG_PCI_DEV_NUM,
		.pci_devs = vm0_pci_devs,
#ifdef VM0_PASSTHROUGH_TPM
		.pt_tpm2 = true,
		.mmiodevs[0] =
			{
				.name = "tpm2",
				.res[0] =
					{
						.user_vm_pa = VM0_TPM_BUFFER_BASE_ADDR_GPA,
						.host_pa = VM0_TPM_BUFFER_BASE_ADDR,
						.size = VM0_TPM_BUFFER_SIZE,
						.mem_type = EPT_UNCACHED,
					},
				.res[1] =
					{
						.user_vm_pa = VM0_TPM_EVENTLOG_BASE_ADDR,
						.host_pa = VM0_TPM_EVENTLOG_BASE_ADDR_HPA,
						.size = VM0_TPM_EVENTLOG_SIZE,
						.mem_type = EPT_WB,
					},
			},
#endif
#ifdef P2SB_BAR_ADDR
		.pt_p2sb_bar = true,
		.mmiodevs[0] =
			{
				.res[0] =
					{
						.user_vm_pa = P2SB_BAR_ADDR_GPA,
						.host_pa = P2SB_BAR_ADDR,
						.size = P2SB_BAR_SIZE,
					},
			},
#endif
		.pt_intx_num = 0,
		.pt_intx = vm0_pt_intx,
	},
	{
		/* Static configured VM1 */
		CONFIG_PRE_STD_VM,
		.name = "PRE_STD_VM1",
		.vm_prio = PRIO_LOW,
		.companion_vm_id = 65535U,
		.guest_flags = (GUEST_FLAG_STATIC_VM),
		.cpu_affinity = VM1_CONFIG_CPU_AFFINITY,
		.memory =
			{
				.region_num = 1,
				.host_regions = vm1_hpa,
			},
		.os_config =
			{
				.name = "",
				.kernel_type = KERNEL_BZIMAGE,
				.kernel_mod_tag = "Linux_bzImage",
				.ramdisk_mod_tag = "",
				.bootargs = VM1_BOOT_ARGS,
			},
		.acpi_config =
			{
				.acpi_mod_tag = "ACPI_VM1",
			},
		.vuart[0] =
			{
				.type = VUART_LEGACY_PIO,
				.addr.port_base = 0x3F8U,
				.irq = 4U,
			},
		.vuart[1] =
			{
				.irq = 3U,
				.type = VUART_LEGACY_PIO,
				.t_vuart.vm_id = 0U,
				.t_vuart.vuart_id = 1U,
				.addr.port_base = 0x2F8U,
			},
		.pci_dev_num = VM1_CONFIG_PCI_DEV_NUM,
		.pci_devs = vm1_pci_devs,
		.pt_intx_num = 0,
		.pt_intx = vm1_pt_intx,
	},
	{
		/* Dynamic configured  VM2 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM3 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM4 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM5 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM6 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM7 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM8 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM9 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM10 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM11 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM12 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM13 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM14 */
		CONFIG_POST_STD_VM,
	},
	{
		/* Dynamic configured  VM15 */
		CONFIG_POST_STD_VM,
	}

};
