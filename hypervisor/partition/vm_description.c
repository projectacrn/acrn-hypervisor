/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static struct vpci_vdev_array vpci_vdev_array1 = {
	.num_pci_vdev = 2,

	.vpci_vdev_list = {
	 {/*vdev 0: hostbridge */
	  .vbdf = PCI_BDF(0x00U, 0x00U, 0x00U),
	  .ops = NULL,
	  .bar = {}, /* don't care for hostbridge */
	  .pdev = {} /* don't care for hostbridge */
	 },

	 {/*vdev 1*/
	  .vbdf = PCI_BDF(0x00U, 0x01U, 0x00U),
	  .ops = &pci_ops_vdev_pt,
	  .bar = {
		[0] = {
		.base = 0UL,
		.size = ALIGN_UP_4K(0x100UL),
		.type = PCIM_BAR_MEM_32
		},
		[5] = {
		.base = 0UL,
		.size = ALIGN_UP_4K(0x2000UL),
		.type = PCIM_BAR_MEM_32
		},
	  },
	 .pdev = {
		.bdf = PCI_BDF(0x00U, 0x01U, 0x00U),
		.bar = {
			[0] = {
			.base = 0xa9000000UL,
			.size = 0x100UL,
			.type = PCIM_BAR_MEM_32
			},
			[5] = {
			.base = 0x1a0000000UL,
			.size = 0x2000UL,
			.type = PCIM_BAR_MEM_64
			},
			}
		}
	 },
	}
};


static struct vpci_vdev_array vpci_vdev_array2 = {
	.num_pci_vdev = 2,

	.vpci_vdev_list = {
	 {/*vdev 0: hostbridge*/
	  .vbdf = PCI_BDF(0x00U, 0x00U, 0x00U),
	  .ops = NULL,
	  .bar = {}, /* don't care for hostbridge */
	  .pdev = {} /* don't care for hostbridge */
	 },

	 {/*vdev 1*/
	  .vbdf = PCI_BDF(0x00U, 0x01U, 0x00U),
	  .ops = &pci_ops_vdev_pt,
	  .bar = {
		[0] = {
		.base = 0UL,
		.size = ALIGN_UP_4K(0x100UL),
		.type = PCIM_BAR_MEM_32
		},
		[5] = {
		.base = 0UL,
		.size = ALIGN_UP_4K(0x2000UL),
		.type = PCIM_BAR_MEM_32
		},
	 },
	 .pdev = {
		.bdf = PCI_BDF(0x00U, 0x02U, 0x00U),
		.bar = {
			[0] = {
			.base = 0xa8000000UL,
			.size = 0x100UL,
			.type = PCIM_BAR_MEM_32
			},
			[5] = {
			.base = 0x1b0000000UL,
			.size = 0x2000UL,
			.type = PCIM_BAR_MEM_64
			},
			}
		}
	 },
	}
};

/*******************************/
/* User Defined VM definitions */
/*******************************/
const struct vm_description_array vm_desc_partition = {
	/* Number of user virtual machines */
	.num_vm_desc = 2,

	.vm_desc_array = {
		{
			/* vm1 */
			.vm_hw_num_cores = 2,
			.vpci_vdev_array = &vpci_vdev_array1,
		},

		{
			/* vm2 */
			.vm_hw_num_cores = 2,
			.vpci_vdev_array = &vpci_vdev_array2,
		},
	}
};

const struct vm_description_array *get_vm_desc_base(void)
{
	return &vm_desc_partition;
}

