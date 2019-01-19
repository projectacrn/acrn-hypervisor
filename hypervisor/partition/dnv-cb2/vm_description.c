/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <e820.h>

/* Number of CPUs in VM1*/
#define VM1_NUM_CPUS    4U

/* Logical CPU IDs assigned to this VM */
uint16_t VM1_CPUS[VM1_NUM_CPUS] = {0U, 2U, 4U, 6U};

/* Number of CPUs in VM2*/
#define VM2_NUM_CPUS    4U

/* Logical CPU IDs assigned with this VM */
uint16_t VM2_CPUS[VM2_NUM_CPUS] = {7U, 5U, 3U, 1U};

static struct vpci_vdev_array vpci_vdev_array1 = {
	.num_pci_vdev = 3,
	.vpci_vdev_list = {
		{/*vdev 0: hostbridge */
			.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
			.ops = &pci_ops_vdev_hostbridge,
			.bar = {},
			.pdev = {
				.bdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
			}
		},

		{/*vdev 1: Ethernet*/
			.vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x0U},
			.ops = &pci_ops_vdev_pt,
			.bar = {
				[0] = {
					.base = 0UL,
					.size = 0x200000UL,
					.type = PCIBAR_MEM32,
				},
				[4] = {
					.base = 0UL,
					.size = 0x4000UL,
					.type = PCIBAR_MEM32,
				},
			},
			.pdev = {
				.bdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x1U},
				.bar = {
					[0] = {
						.base = 0x80C00000,
						.size = 0x200000UL,
						.type = PCIBAR_MEM32,
					},
					[4] = {
						.base = 0x81000000,
						.size = 0x4000UL,
						.type = PCIBAR_MEM32,
					},
				}
			}
		},

		{/*vdev 2: USB*/
			.vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x0U},
			.ops = &pci_ops_vdev_pt,
			.bar = {
				[0] = {
					.base = 0UL,
					.size = 0x10000UL,
					.type = PCIBAR_MEM32,
				}
			},
			.pdev = {
				.bdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x0U},
				.bar = {
					[0] = {
						.base = 0x81340000,
						.size = 0x10000UL,
						.type = PCIBAR_MEM32,
					}
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

		{/*vdev 1: SATA controller*/
			.vbdf.bits = {.b = 0x00U, .d = 0x05U, .f = 0x0U},
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
				.bdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x0U},
				.bar = {
					[0] = {
						.base = 0x81354000,
						.size = 0x2000UL,
						.type = PCIBAR_MEM32
					},
					[1] = {
						.base = 0x8135f000,
						.size = 0x1000UL,
						.type = PCIBAR_MEM32
					},
					[5] = {
						.base = 0x8135e000,
						.size = 0x1000UL,
						.type = PCIBAR_MEM32
					},
				}
			}
		},

		{/*vdev 2: Ethernet*/
			.vbdf.bits = {.b = 0x00U, .d = 0x06U, .f = 0x0U},
			.ops = &pci_ops_vdev_pt,
			.bar = {
				[0] = {
					.base = 0UL,
					.size = 0x200000UL,
					.type = PCIBAR_MEM32,
				},
				[4] = {
					.base = 0UL,
					.size = 0x4000UL,
					.type = PCIBAR_MEM32,
				},
			},

			.pdev = {
				.bdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x0U},
				.bar = {
					[0] = {
						.base = 0x80e00000,
						.size = 0x200000UL,
						.type = PCIBAR_MEM32,
					},
					[4] = {
						.base = 0x81004000,
						.size = 0x4000UL,
						.type = PCIBAR_MEM32,
					}
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
				/* Internal variable, MUSTBE init to -1 */
				.vm_hw_num_cores = VM1_NUM_CPUS,
				.vm_pcpu_ids = &VM1_CPUS[0],
				.start_hpa = 0x100000000UL,
				.mem_size = 0x80000000UL, /* uses contiguous memory from host */
				.vm_vuart = true,
				.bootargs = "root=/dev/sda rw rootwait noxsave maxcpus=4 nohpet console=hvc0 " \
						"console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M "\
						"consoleblank=0 tsc=reliable xapic_phys  apic_debug",
				.vpci_vdev_array = &vpci_vdev_array1,
				.lapic_pt = true,
			},

			{
				.type = PRE_LAUNCHED_VM,
				/* Internal variable, MUSTBE init to -1 */
				.vm_hw_num_cores = VM2_NUM_CPUS,
				.vm_pcpu_ids = &VM2_CPUS[0],
				.start_hpa = 0x180000000UL,
				.mem_size = 0x80000000UL, /* uses contiguous memory from host */
				.vm_vuart = true,
				.bootargs = "root=/dev/sda2 rw rootwait noxsave maxcpus=4 nohpet console=hvc0 "\
						"console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M "\
						"consoleblank=0 tsc=reliable xapic_phys apic_debug",
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
		.is_bsp = false,
	},
	{
		.vm_config_ptr = &vm_config_partition.vm_config_array[0],
		.is_bsp = false,
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
		.length   =  0x7FF00000U,
		.type     =  E820_TYPE_RAM
	},

	{	/* lowmem to PCI hole */
		.baseaddr =  0x80000000U,
		.length   =  0x40000000U,
		.type     =  E820_TYPE_RESERVED
	},

	{	/* PCI hole to 4G */
		.baseaddr =  0xe0000000U,
		.length   =  0x20000000U,
		.type     =  E820_TYPE_RESERVED
	},
};
