/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <vuart.h>
#include <pci_dev.h>

struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{
		.load_order = SOS_VM,
		.name = "ACRN SOS VM",
		.uuid = {0xdbU, 0xbbU, 0xd4U, 0x34U, 0x7aU, 0x57U, 0x42U, 0x16U,	\
			 0xa1U, 0x2cU, 0x22U, 0x01U, 0xf1U, 0xabU, 0x02U, 0x40U},
			/* dbbbd434-7a57-4216-a12c-2201f1ab0240 */

		/* Allow SOS to reboot the host since there is supposed to be the highest severity guest */
		.guest_flags = GUEST_FLAG_HIGHEST_SEVERITY,
		.clos = 0U,
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
			.addr.port_base = CONFIG_COM_BASE,
			.irq = CONFIG_COM_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
		.pci_dev_num = SOS_EMULATED_PCI_DEV_NUM,
		.pci_devs = sos_pci_devs,
	},
	{
		.load_order = POST_LAUNCHED_VM,
		.uuid = {0xd2U, 0x79U, 0x54U, 0x38U, 0x25U, 0xd6U, 0x11U, 0xe8U,	\
			 0x86U, 0x4eU, 0xcbU, 0x7aU, 0x18U, 0xb3U, 0x46U, 0x43U},
			/* d2795438-25d6-11e8-864e-cb7a18b34643 */
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		}

	},
#if CONFIG_MAX_KATA_VM_NUM > 0
	{
		.load_order = POST_LAUNCHED_VM,
		.uuid = {0xa7U, 0xadU, 0xa5U, 0x06U, 0x1aU, 0xb0U, 0x4bU, 0x6bU,	\
			 0xa0U, 0xdaU, 0xe5U, 0x13U, 0xcaU, 0x9bU, 0x8cU, 0x2fU},
			/* a7ada506-1ab0-4b6b-a0da-e513ca9b8c2f */
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		}
	},
#endif
};
