/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <pci_devices.h>
#include <vpci.h>
#include <vbar_base.h>
#include <mmu.h>
#include <page.h>

#define PTDEV(PCI_DEV)		PCI_DEV, PCI_DEV##_VBAR

struct acrn_vm_pci_dev_config vm0_pci_devs[VM0_CONFIG_PCI_DEV_NUM] = {
	{
		.emu_type = PCI_DEV_TYPE_HVEMUL,
		.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
		.vdev_ops = &vhostbridge_ops,
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV,
		.vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x00U},
		PTDEV(SATA_CONTROLLER_0),
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV,
		.vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U},
		PTDEV(ETHERNET_CONTROLLER_0),
	},
};

struct acrn_vm_pci_dev_config vm1_pci_devs[VM1_CONFIG_PCI_DEV_NUM] = {
	{
		.emu_type = PCI_DEV_TYPE_HVEMUL,
		.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
		.vdev_ops = &vhostbridge_ops,
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV,
		.vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x00U},
		PTDEV(USB_CONTROLLER_0),
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV,
		.vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U},
		PTDEV(NETWORK_CONTROLLER_0),
	},
};
