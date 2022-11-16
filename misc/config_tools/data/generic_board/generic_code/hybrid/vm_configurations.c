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

extern struct pt_intx_config vm0_pt_intx[1U];
static struct vm_hpa_regions vm0_hpa[] = {
	{.start_hpa = 0x100000000, .size_hpa = 0x20000000},
	{.start_hpa = 0x120000000, .size_hpa = 0x10000000},
};
extern struct acrn_vm_pci_dev_config sos_pci_devs[CONFIG_MAX_PCI_DEV_NUM];
struct acrn_vm_config
	vm_configs[CONFIG_MAX_VM_NUM] =
		{{
			 /* Static configured VM0 */
			 CONFIG_PRE_STD_VM,
			 .name = "SAFETY_VM0",
			 .vm_prio = PRIO_LOW,
			 .companion_vm_id = 65535U,
			 .guest_flags = (GUEST_FLAG_STATIC_VM),
			 .cpu_affinity = VM0_CONFIG_CPU_AFFINITY,
			 .memory =
				 {
					 .region_num = 2,
					 .host_regions = vm0_hpa,
				 },
			 .os_config =
				 {
					 .name = "",
					 .kernel_type = KERNEL_ELF,
					 .kernel_mod_tag = "Zephyr_ElfImage",
					 .ramdisk_mod_tag = "",
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
			 .vuart[2] =
				 {
					 .irq = 3U,
					 .type = VUART_LEGACY_PIO,
					 .t_vuart.vm_id = 1U,
					 .t_vuart.vuart_id = 2U,
					 .addr.port_base = 0x2F8U,
				 },
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
				CONFIG_SERVICE_VM,
				.name = "ACRN_Service_VM",
				/* Allow Service VM to reboot the system since it is the highest priority VM. */
				.vm_prio = PRIO_LOW,
				.companion_vm_id = 65535U,
				.guest_flags = (GUEST_FLAG_STATIC_VM),
				.cpu_affinity = SERVICE_VM_CONFIG_CPU_AFFINITY,
				.os_config =
					{
						.name = "",
						.kernel_type = KERNEL_BZIMAGE,
						.kernel_mod_tag = "Linux_bzImage",
						.ramdisk_mod_tag = "",
						.bootargs = SERVICE_VM_OS_BOOTARGS,
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
				.vuart[2] =
					{
						.irq = 0U,
						.type = VUART_LEGACY_PIO,
						.addr.port_base = 0X9000U,
						.t_vuart.vm_id = 0U,
						.t_vuart.vuart_id = 2U,
					},
				.vuart[3] =
					{
						.irq = 0U,
						.type = VUART_LEGACY_PIO,
						.addr.port_base = 0X9008U,
						.t_vuart.vm_id = 2U,
						.t_vuart.vuart_id = 1U,
					},
				.vuart[4] =
					{
						.irq = 0U,
						.type = VUART_LEGACY_PIO,
						.addr.port_base = 0X9010U,
						.t_vuart.vm_id = 3U,
						.t_vuart.vuart_id = 1U,
					},
				.pci_dev_num = 0U,
				.pci_devs = sos_pci_devs,
			},
			{
				/* Static configured VM2 */
				CONFIG_POST_STD_VM,
				.name = "POST_STD_VM1",
				.vm_prio = PRIO_LOW,
				.companion_vm_id = 65535U,
				.guest_flags = (GUEST_FLAG_STATIC_VM),
				.cpu_affinity = VM2_CONFIG_CPU_AFFINITY,
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
						.t_vuart.vm_id = 1U,
						.t_vuart.vuart_id = 3U,
						.addr.port_base = 0x2F8U,
					},
			},
			{
				/* Static configured VM3 */
				CONFIG_POST_STD_VM,
				.name = "POST_STD_VM2",
				.vm_prio = PRIO_LOW,
				.companion_vm_id = 65535U,
				.guest_flags = (GUEST_FLAG_STATIC_VM),
				.cpu_affinity = VM3_CONFIG_CPU_AFFINITY,
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
						.t_vuart.vm_id = 1U,
						.t_vuart.vuart_id = 4U,
						.addr.port_base = 0x2F8U,
					},
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
