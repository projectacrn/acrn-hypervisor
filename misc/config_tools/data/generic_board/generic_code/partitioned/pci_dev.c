/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/vm_config.h>
#include <pci_devices.h>
#include <vpci.h>
#include <vbar_base.h>
#include <asm/mmu.h>
#include <asm/page.h>

/*
 * TODO: remove PTDEV macro and add DEV_PRIVINFO macro to initialize pbdf for
 * passthrough device configuration and shm_name for ivshmem device configuration.
 */
#define PTDEV(PCI_DEV)		PCI_DEV, PCI_DEV##_VBAR

/*
 * TODO: add DEV_PCICOMMON macro to initialize emu_type, vbdf and vdev_ops
 * to simplify the code.
 */

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
		PTDEV(USB_CONTROLLER_3),
	},
};
