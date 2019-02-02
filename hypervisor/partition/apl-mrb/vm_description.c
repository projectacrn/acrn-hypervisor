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
	  .pdev = {
		 .bdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
		}
	 },

	 {/*vdev 1: SATA controller*/
	  .vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x0U},
	  .ops = &pci_ops_vdev_pt,
	 .pdev = {
		.bdf.bits = {.b = 0x00U, .d = 0x12U, .f = 0x0U},
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
	  .pdev = {
			.bdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x0U},
		}
	 },

	 {/*vdev 1: USB controller*/
	  .vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x0U},
	  .ops = &pci_ops_vdev_pt,
	 .pdev = {
		.bdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x0U},
		}
	 },

	 {/*vdev 2: Ethernet*/
	  .vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x0U},
	  .ops = &pci_ops_vdev_pt,
	 .pdev = {
		.bdf.bits = {.b = 0x02U, .d = 0x00U, .f = 0x0U},
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
				.guest_flags = IO_COMPLETION_POLLING,
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
				.guest_flags = LAPIC_PASSTHROUGH | IO_COMPLETION_POLLING,
				.memory.start_hpa = 0x120000000UL,
				.memory.size = 0x20000000UL, /* uses contiguous memory from host */
				.vm_vuart = true,
				.os_config.bootargs = "root=/dev/sda3 rw rootwait noxsave maxcpus=2 nohpet console=hvc0 \
						console=ttyS2 no_timer_check ignore_loglevel log_buf_len=16M \
						consoleblank=0 tsc=reliable xapic_phys",
				.vpci_vdev_array = &vpci_vdev_array2,
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
