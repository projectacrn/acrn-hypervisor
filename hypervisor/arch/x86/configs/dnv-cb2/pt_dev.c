/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

struct acrn_vm_pci_ptdev_config vm0_pci_ptdevs[3] = {
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
		.pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
	},
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x00U},
		.pbdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x01U},
	},
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U},
		.pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x00U},
	},
};

struct acrn_vm_pci_ptdev_config vm1_pci_ptdevs[3] = {
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
		.pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
	},
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x00U},
		.pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U},
	},
	{
		.vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U},
		.pbdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x00U},
	},
};
