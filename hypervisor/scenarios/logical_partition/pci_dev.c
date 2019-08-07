/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <pci_devices.h>

/* The vbar_base info of pt devices is included in device MACROs which defined in
 *           arch/x86/configs/$(CONFIG_BOARD)/pci_devices.h.
 * The memory range of vBAR should exactly match with the e820 layout of VM.
 */

struct acrn_vm_pci_dev_config vm0_pci_devs[VM0_CONFIG_PCI_DEV_NUM] = {
	{
		.emu_type = PCI_DEV_TYPE_HVEMUL,
		.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV,
		.vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x00U},
		VM0_STORAGE_CONTROLLER
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV,
		.vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U},
		VM0_NETWORK_CONTROLLER
	},
};

struct acrn_vm_pci_dev_config vm1_pci_devs[VM1_CONFIG_PCI_DEV_NUM] = {
	{
		.emu_type = PCI_DEV_TYPE_HVEMUL,
		.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV,
		.vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x00U},
		VM1_STORAGE_CONTROLLER
	},
#if defined(VM1_NETWORK_CONTROLLER)
	{
		.emu_type = PCI_DEV_TYPE_PTDEV,
		.vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U},
		VM1_NETWORK_CONTROLLER
	},
#endif
};
