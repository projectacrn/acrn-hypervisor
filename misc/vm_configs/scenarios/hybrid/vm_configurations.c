/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <vm_config.h>
#include <vuart.h>
#include <pci_dev.h>

struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{	/* VM0 */
		CONFIG_SAFETY_VM(1),
		.name = "ACRN PRE-LAUNCHED VM0",
		.cpu_affinity = VM0_CONFIG_CPU_AFFINITY,
		.guest_flags = 0UL,
		.memory = {
			.start_hpa = VM0_CONFIG_MEM_START_HPA,
			.size = VM0_CONFIG_MEM_SIZE,
			.start_hpa2 = VM0_CONFIG_MEM_START_HPA2,
			.size_hpa2 = VM0_CONFIG_MEM_SIZE_HPA2,
		},
		.os_config = {
			.name = "Zephyr",
			.kernel_type = KERNEL_ZEPHYR,
			.kernel_mod_tag = "Zephyr_RawImage",
			.kernel_load_addr = 0x8000,
			.kernel_entry_addr = 0x8000,
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
#ifdef P2SB_BAR_ADDR
		.pt_p2sb_bar = true,
		.mmiodevs[0] = {
			.base_gpa = 0xFD000000UL,
			.base_hpa = P2SB_BAR_ADDR,
			.size = 0x1000000UL,
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
