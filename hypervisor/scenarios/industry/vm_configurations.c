/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <vuart.h>

struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{
		.load_order = SOS_VM,
		.name = "ACRN SOS VM",
		.uuid = {0xdbU, 0xbbU, 0xd4U, 0x34U, 0x7aU, 0x57U, 0x42U, 0x16U,	\
			 0xa1U, 0x2cU, 0x22U, 0x01U, 0xf1U, 0xabU, 0x02U, 0x40U},
			/* dbbbd434-7a57-4216-a12c-2201f1ab0240 */
		.guest_flags = 0UL,
		.clos = 0U,
		.memory = {
			.start_hpa = 0UL,
			.size = CONFIG_SOS_RAM_SIZE,
		},
		.os_config = {
			.name = "ACRN Service OS",
			.kernel_type = KERNEL_BZIMAGE,
		},
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = CONFIG_COM_BASE,
			.irq = CONFIG_COM_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		}
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
	{
		.load_order = POST_LAUNCHED_VM,
		.uuid = {0x49U, 0x5aU, 0xe2U, 0xe5U, 0x26U, 0x03U, 0x4dU, 0x64U,	\
			 0xafU, 0x76U, 0xd4U, 0xbcU, 0x5aU, 0x8eU, 0xc0U, 0xe5U},
			/* 495ae2e5-2603-4d64-af76-d4bc5a8ec0e5 */

		/* The hard RTVM must be launched as VM2 */
		.guest_flags = GUEST_FLAG_HIGHEST_SEVERITY,
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
