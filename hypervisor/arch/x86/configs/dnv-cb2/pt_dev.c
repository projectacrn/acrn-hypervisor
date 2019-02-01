/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

struct vpci_vdev_array vpci_vdev_array0 = {
	.num_pci_vdev = 3,

	.vpci_vdev_list = {
		{/*vdev 0: hostbridge */
			.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
			.pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
		},

		{/*vdev 1: Ethernet*/
			.vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x0U},
			.pbdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x1U},
		},

		{/*vdev 2: USB*/
			.vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x0U},
			.pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x0U},
		},
	}
};

struct vpci_vdev_array vpci_vdev_array1 = {
	.num_pci_vdev = 3,

	.vpci_vdev_list = {
		{/*vdev 0: hostbridge*/
			.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
			.pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
		},

		{/*vdev 1: SATA controller*/
			.vbdf.bits = {.b = 0x00U, .d = 0x05U, .f = 0x0U},
			.pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x0U},
		},

		{/*vdev 2: Ethernet*/
			.vbdf.bits = {.b = 0x00U, .d = 0x06U, .f = 0x0U},
			.pbdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x0U},
		},
	}
};
