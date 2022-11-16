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

extern struct acrn_vm_pci_dev_config sos_pci_devs[CONFIG_MAX_PCI_DEV_NUM];
struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{
		/* Static configured VM0 */
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
				.addr.port_base = 0x2F8U,
				.t_vuart.vm_id = 1U,
				.t_vuart.vuart_id = 1U,
			},
		.vuart[2] =
			{
				.irq = 4U,
				.type = VUART_LEGACY_PIO,
				.addr.port_base = 0x3E8U,
				.t_vuart.vm_id = 2U,
				.t_vuart.vuart_id = 1U,
			},
		.vuart[3] =
			{
				.irq = 0U,
				.type = VUART_LEGACY_PIO,
				.addr.port_base = 0X9000U,
				.t_vuart.vm_id = 1U,
				.t_vuart.vuart_id = 2U,
			},
		.vuart[4] =
			{
				.irq = 0U,
				.type = VUART_LEGACY_PIO,
				.addr.port_base = 0X9008U,
				.t_vuart.vm_id = 2U,
				.t_vuart.vuart_id = 2U,
			},
		.pci_dev_num = 0U,
		.pci_devs = sos_pci_devs,
	},
	{
		/* Static configured VM1 */
		CONFIG_POST_STD_VM,
		.name = "POST_STD_VM1",
		.vm_prio = PRIO_LOW,
		.companion_vm_id = 65535U,
		.guest_flags = (GUEST_FLAG_STATIC_VM),
		.cpu_affinity = VM1_CONFIG_CPU_AFFINITY,
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
				.irq = 3U,
				.type = VUART_LEGACY_PIO,
				.t_vuart.vm_id = 0U,
				.t_vuart.vuart_id = 3U,
				.addr.port_base = 0x2F8U,
			},
	},
	{
		/* Static configured VM2 */
		CONFIG_POST_RT_VM,
		.name = "POST_RT_VM1",
		.vm_prio = PRIO_LOW,
		.companion_vm_id = 65535U,
		.guest_flags = (GUEST_FLAG_LAPIC_PASSTHROUGH | GUEST_FLAG_RT | GUEST_FLAG_STATIC_VM),
		.cpu_affinity = VM2_CONFIG_CPU_AFFINITY,
		.vuart[0] =
			{
				.type = VUART_LEGACY_PIO,
				.addr.port_base = 0x3F8U,
				.irq = 4U,
			},
		.vuart[1] =
			{
				.irq = 4U,
				.type = VUART_LEGACY_PIO,
				.t_vuart.vm_id = 0U,
				.t_vuart.vuart_id = 2U,
				.addr.port_base = 0x3E8U,
			},
		.vuart[2] =
			{
				.irq = 3U,
				.type = VUART_LEGACY_PIO,
				.t_vuart.vm_id = 0U,
				.t_vuart.vuart_id = 4U,
				.addr.port_base = 0x2F8U,
			},
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
