/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/vm_config.h>
#include <vpci.h>
#include <asm/mmu.h>
#include <asm/page.h>
#include <vmcs9900.h>
#include <ivshmem_cfg.h>
#define INVALID_PCI_BASE 0U
struct acrn_vm_pci_dev_config vm0_pci_devs[VM0_CONFIG_PCI_DEV_NUM] = {
	{
		.emu_type = PCI_DEV_TYPE_HVEMUL,
		.vdev_ops = &vhostbridge_ops,
		.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV,
		.vbdf.bits =
			{
				.b = 0x00U,
				.d = 0x01U,
				.f = 0x00U,
			},
		.pbdf.bits =
			{
				.b = 0x00U,
				.d = 0x17U,
				.f = 0x00U,
			},
		.vbar_base[0] = 0x80020000UL,
		.vbar_base[5] = 0x80022000UL,
		.vbar_base[1] = 0x80023000UL,
		.vbar_base[4] = 0x3060UL,
		.vbar_base[3] = 0x3080UL,
		.vbar_base[2] = 0x3090UL,
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV,
		.vbdf.bits =
			{
				.b = 0x00U,
				.d = 0x02U,
				.f = 0x00U,
			},
		.pbdf.bits =
			{
				.b = 0x00U,
				.d = 0x1FU,
				.f = 0x06U,
			},
		.vbar_base[0] = 0x80000000UL,
	},
};
struct acrn_vm_pci_dev_config vm1_pci_devs[VM1_CONFIG_PCI_DEV_NUM] = {
	{
		.emu_type = PCI_DEV_TYPE_HVEMUL,
		.vdev_ops = &vhostbridge_ops,
		.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
	},
	{
		.emu_type = PCI_DEV_TYPE_PTDEV,
		.vbdf.bits =
			{
				.b = 0x00U,
				.d = 0x01U,
				.f = 0x00U,
			},
		.pbdf.bits =
			{
				.b = 0x00U,
				.d = 0x14U,
				.f = 0x00U,
			},
		.vbar_base[0] = 0x80000000UL,
	},
};
