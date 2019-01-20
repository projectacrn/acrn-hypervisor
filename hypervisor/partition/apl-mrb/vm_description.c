/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <e820.h>

static struct vpci_vdev_array vpci_vdev_array1 = {
	.num_pci_vdev = 2,

	.vpci_vdev_list = {
	 {/*vdev 0: hostbridge */
	  .vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
	  .ops = &pci_ops_vdev_hostbridge,
	  .bar = {},
	  .pdev = {
		 .bdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
		}
	 },

	 {/*vdev 1: SATA controller*/
	  .vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x0U},
	  .ops = &pci_ops_vdev_pt,
	  .bar = {
			[0] = {
			.base = 0UL,
			.size = 0x2000UL,
			.type = PCIBAR_MEM32
			},
			[1] = {
			.base = 0UL,
			.size = 0x1000UL,
			.type = PCIBAR_MEM32
			},
			[5] = {
			.base = 0UL,
			.size = 0x1000UL,
			.type = PCIBAR_MEM32
			},
	  },
	 .pdev = {
		.bdf.bits = {.b = 0x00U, .d = 0x12U, .f = 0x0U},
		.bar = {
			[0] = {
			.base = 0xb3f10000UL,
			.size = 0x2000UL,
			.type = PCIBAR_MEM32
			},
			[1] = {
			.base = 0xb3f53000UL,
			.size = 0x100UL,
			.type = PCIBAR_MEM32
			},
			[5] = {
			.base = 0xb3f52000UL,
			.size = 0x800UL,
			.type = PCIBAR_MEM32
			},
		 }
		}
	 },
	}
};

static struct vpci_vdev_array vpci_vdev_array2 = {
	.num_pci_vdev = 3,

	.vpci_vdev_list = {
	 {/*vdev 0: hostbridge*/
	  .vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
	  .ops = &pci_ops_vdev_hostbridge,
	  .bar = {},
	  .pdev = {
			.bdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
		}
	 },

	 {/*vdev 1: USB controller*/
	  .vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x0U},
	  .ops = &pci_ops_vdev_pt,
	  .bar = {
			[0] = {
			.base = 0UL,
			.size = 0x10000UL,
			.type = PCIBAR_MEM32
		 },
	  },
	 .pdev = {
		.bdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x0U},
		.bar = {
			[0] = {
			.base = 0xb3f00000UL,
			.size = 0x10000UL,
			.type = PCIBAR_MEM64
			},
		 }
		}
	 },

	 {/*vdev 2: Ethernet*/
	  .vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x0U},
	  .ops = &pci_ops_vdev_pt,
	  .bar = {
			[0] = {
			.base = 0UL,
			.size = 0x80000UL,
			.type = PCIBAR_MEM32
			},
			[3] = {
			.base = 0UL,
			.size = 0x4000UL,
			.type = PCIBAR_MEM32
			},
	  },
	 .pdev = {
		.bdf.bits = {.b = 0x02U, .d = 0x00U, .f = 0x0U},
		.bar = {
			[0] = {
			.base = 0xb3c00000UL,
			.size = 0x80000UL,
			.type = PCIBAR_MEM32
			},
			[3] = {
			.base = 0xb3c80000UL,
			.size = 0x4000UL,
			.type = PCIBAR_MEM32
			 },
		 }
		}
	 },
	}
};

/*******************************/
/* User Defined VM definitions */
/*******************************/
struct vm_config_arraies vm_config_partition = {

		/* Virtual Machine descriptions */
		.vm_config_array = {
			{
				.type = PRE_LAUNCHED_VM,
				.pcpu_bitmap = (PLUG_CPU(0) | PLUG_CPU(2)),
				.memory.start_hpa = 0x100000000UL,
				.memory.size = 0x20000000UL, /* uses contiguous memory from host */
				.vm_vuart = true,
				.os_config.bootargs = "root=/dev/sda3 rw rootwait noxsave maxcpus=2 nohpet console=hvc0 \
						console=ttyS2 no_timer_check ignore_loglevel log_buf_len=16M \
						consoleblank=0 tsc=reliable xapic_phys",
				.vpci_vdev_array = &vpci_vdev_array1,
			},

			{
				.type = PRE_LAUNCHED_VM,
				.pcpu_bitmap = (PLUG_CPU(1) | PLUG_CPU(3)),
				.memory.start_hpa = 0x120000000UL,
				.memory.size = 0x20000000UL, /* uses contiguous memory from host */
				.vm_vuart = true,
				.os_config.bootargs = "root=/dev/sda3 rw rootwait noxsave maxcpus=2 nohpet console=hvc0 \
						console=ttyS2 no_timer_check ignore_loglevel log_buf_len=16M \
						consoleblank=0 tsc=reliable xapic_phys",
				.vpci_vdev_array = &vpci_vdev_array2,
				.lapic_pt = true,
			},
		}
};

const struct pcpu_vm_config_mapping pcpu_vm_config_map[] = {
	{
		.vm_config_ptr = &vm_config_partition.vm_config_array[0],
		.is_bsp = true,
	},
	{
		.vm_config_ptr = &vm_config_partition.vm_config_array[1],
		.is_bsp = false,
	},
	{
		.vm_config_ptr = &vm_config_partition.vm_config_array[0],
		.is_bsp = false,
	},
	{
		.vm_config_ptr = &vm_config_partition.vm_config_array[1],
		.is_bsp = true,
	},
};

const struct e820_entry e820_default_entries[NUM_E820_ENTRIES] = {
	{	/* 0 to mptable */
		.baseaddr =  0x0U,
		.length   =  0xEFFFFU,
		.type     =  E820_TYPE_RAM
	},

	{	/* mptable 65536U */
		.baseaddr =  0xF0000U,
		.length   =  0x10000U,
		.type     =  E820_TYPE_RESERVED
	},

	{	/* mptable to lowmem */
		.baseaddr =  0x100000U,
		.length   =  0x1FF00000U,
		.type     =  E820_TYPE_RAM
	},

	{	/* lowmem to PCI hole */
		.baseaddr =  0x20000000U,
		.length   =  0xa0000000U,
		.type     =  E820_TYPE_RESERVED
	},

	{	/* PCI hole to 4G */
		.baseaddr =  0xe0000000U,
		.length   =  0x20000000U,
		.type     =  E820_TYPE_RESERVED
	},
};
