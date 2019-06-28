/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <pci_devices.h>

struct acrn_vm_pci_ptdev_config vm0_pci_ptdevs[VM0_CONFIG_PCI_PTDEV_NUM] = {
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
		HOST_BRIDGE
	},
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x00U},
		.vbar_base[0] = 0xc0084000UL,
		.vbar_base[1] = 0xc0086000UL,
		.vbar_base[5] = 0xc0087000UL,
		VM0_STORAGE_CONTROLLER
	},
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U},
		.vbar_base[0] = 0xc0000000UL,
		.vbar_base[3] = 0xc0080000UL,
		VM0_NETWORK_CONTROLLER
	},
};

struct acrn_vm_pci_ptdev_config vm1_pci_ptdevs[VM1_CONFIG_PCI_PTDEV_NUM] = {
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
		HOST_BRIDGE
	},
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x00U},
		.vbar_base[0] = 0xc0000000UL,
		VM1_STORAGE_CONTROLLER
	},
#if defined(VM1_NETWORK_CONTROLLER)
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U},
		VM1_NETWORK_CONTROLLER
	},
#endif
};
