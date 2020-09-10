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
#include <ivshmem.h>

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
	{
		.emu_type = PCI_DEV_TYPE_HVEMUL,
		.vbdf.bits = {.b = 0x00U, .d = 0x03U, .f = 0x00U},
		.vdev_ops = &vpci_ivshmem_ops,
		.shm_region_name = IVSHMEM_SHM_REGION_0,
		IVSHMEM_DEVICE_0_VBAR
	},
};

struct acrn_vm_pci_dev_config vm2_pci_devs[VM2_CONFIG_PCI_DEV_NUM] = {
	{
		.emu_type = PCI_DEV_TYPE_HVEMUL,
		.vbdf.value = UNASSIGNED_VBDF,
		.vdev_ops = &vpci_ivshmem_ops,
		.shm_region_name = IVSHMEM_SHM_REGION_0
	},
};
