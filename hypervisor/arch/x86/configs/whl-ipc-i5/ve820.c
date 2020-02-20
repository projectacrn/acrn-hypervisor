/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <e820.h>
#include <vm.h>

#define VM0_VE820_ENTRIES_WHL_IPC_I5	5U
#define VM1_VE820_ENTRIES_WHL_IPC_I5	5U
static const struct e820_entry vm0_ve820_entry[VM0_VE820_ENTRIES_WHL_IPC_I5] = {
	{	/* usable RAM under 1MB */
		.baseaddr = 0x0UL,
		.length   = 0xF0000UL,		/* 960KB */
		.type     = E820_TYPE_RAM
	},

	{	/* mptable */
		.baseaddr = 0xF0000UL,		/* 960KB */
		.length   = 0x10000UL,		/* 64KB */
		.type     = E820_TYPE_RESERVED
	},

	{	/* lowmem */
		.baseaddr = 0x100000UL,		/* 1MB */
		.length   = 0x1ff00000UL,	/* 511.0MB */
		.type     = E820_TYPE_RAM
	},

	{	/* between lowmem and PCI hole */
		.baseaddr = 0x20000000UL,	/* 512.0MB */
		.length   = 0xA0000000UL,	/* 2560.0MB */
		.type     = E820_TYPE_RESERVED
	},

	{	/* between PCI hole and 4 GB */
		.baseaddr = 0xe0000000UL,	/* 3.5GB */
		.length   = 0x20000000UL,	/* 512.0MB */
		.type     = E820_TYPE_RESERVED
	},

};

static const struct e820_entry vm1_ve820_entry[VM1_VE820_ENTRIES_WHL_IPC_I5] = {
	{	/* usable RAM under 1MB */
		.baseaddr = 0x0UL,
		.length   = 0xF0000UL,		/* 960KB */
		.type     = E820_TYPE_RAM
	},

	{	/* mptable */
		.baseaddr = 0xF0000UL,		/* 960KB */
		.length   = 0x10000UL,		/* 64KB */
		.type     = E820_TYPE_RESERVED
	},

	{	/* lowmem */
		.baseaddr = 0x100000UL,		/* 1MB */
		.length   = 0x1ff00000UL,	/* 511.0MB */
		.type     = E820_TYPE_RAM
	},

	{	/* between lowmem and PCI hole */
		.baseaddr = 0x20000000UL,	/* 512.0MB */
		.length   = 0xA0000000UL,	/* 2560.0MB */
		.type     = E820_TYPE_RESERVED
	},

	{	/* between PCI hole and 4 GB */
		.baseaddr = 0xe0000000UL,	/* 3.5GB */
		.length   = 0x20000000UL,	/* 512.0MB */
		.type     = E820_TYPE_RESERVED
	},

};

/**
 * @pre vm != NULL
*/
void create_prelaunched_vm_e820(struct acrn_vm *vm)
{
	if (vm->vm_id == 0x0U)
	{
		vm->e820_entry_num = VM0_VE820_ENTRIES_WHL_IPC_I5;
		vm->e820_entries = vm0_ve820_entry;
	}

	if (vm->vm_id == 0x1U)
	{
		vm->e820_entry_num = VM1_VE820_ENTRIES_WHL_IPC_I5;
		vm->e820_entries = vm1_ve820_entry;
	}

}
