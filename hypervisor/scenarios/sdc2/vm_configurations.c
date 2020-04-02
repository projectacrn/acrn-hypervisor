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
		.guest_flags = 0UL,
		.severity = SEVERITY_SOS,
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
		.vcpu_num = 1U,
		.vcpu_affinity = VM1_CONFIG_VCPU_AFFINITY,
		.severity = SEVERITY_STANDARD_VM,
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		}

	},
		{
		.load_order = POST_LAUNCHED_VM,
		.uuid = {0x61U, 0x5dU, 0xb8U, 0x2aU, 0xe1U, 0x89U, 0x4bU, 0x4fU,	\
			 0x8dU, 0xbbU, 0xd3U, 0x21U, 0x34U, 0x3eU, 0x4aU, 0xb3U},
			/* 615db82a-e189-4b4f-8dbb-d321343e4ab3 */
		.vcpu_num = 1U,
		.vcpu_affinity = VM2_CONFIG_VCPU_AFFINITY,
		.severity = SEVERITY_STANDARD_VM,
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		}

	},
	{
		.load_order = POST_LAUNCHED_VM,
		.uuid = {0x38U, 0x15U, 0x88U, 0x21U, 0x52U, 0x08U, 0x40U, 0x05U,	\
			 0xb7U, 0x2aU, 0x8aU, 0x60U, 0x9eU, 0x41U, 0x90U, 0xd0U},
			/* 38158821-5208-4005-b72a-8a609e4190d0 */
		.vcpu_num = 1U,
		.vcpu_affinity = VM3_CONFIG_VCPU_AFFINITY,
		.severity = SEVERITY_STANDARD_VM,
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		}

	}
};
