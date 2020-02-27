/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <e820.h>
#include <mmu.h>
#include <vm.h>

#define ENTRY_HPA1		2U
#define ENTRY_HPA1_HI		4U

static struct e820_entry pre_vm_e820[PRE_VM_NUM][E820_MAX_ENTRIES];

static const struct e820_entry pre_ve820_template[E820_MAX_ENTRIES] = {
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
	{	/* hpa1 */
		.baseaddr = 0x100000UL,		/* 1MB */
		.length   = (MEM_2G - MEM_1M),
		.type     = E820_TYPE_RAM
	},
	{	/* 32bit PCI hole */
		.baseaddr = 0x80000000UL,	/* 2048MB */
		.length   = MEM_2G,
		.type     = E820_TYPE_RESERVED
	},
};

/**
 * @pre entry != NULL
 */
static inline uint64_t add_ram_entry(struct e820_entry *entry, uint64_t gpa, uint64_t length)
{
	entry->baseaddr = gpa;
	entry->length = length;
	entry->type = E820_TYPE_RAM;
	return round_pde_up(entry->baseaddr + entry->length);
}

/**
 * @pre vm != NULL
 *
 * ve820 layout for pre-launched VM:
 *
 *	 entry0: usable under 1MB
 *	 entry1: reserved for MP Table from 0xf0000 to 0xfffff
 *	 entry2: usable for hpa1 or hpa1_lo from 0x100000
 *	 entry3: reserved for 32bit PCI hole from 0x80000000 to 0xffffffff
 *	 (entry4): usable for
 *                                         a) hpa1_hi, if hpa1 > 2GB
 *                                         b) hpa2, if (hpa1 + hpa2) < 2GB
 *                                         c) hpa2_lo, if hpa1 < 2GB and (hpa1 + hpa2) > 2GB
 *	 (entry5): usable for
 *                                         a) hpa2, if hpa1 > 2GB
 *                                         b) hpa2_hi, if hpa1 < 2GB and (hpa1 + hpa2) > 2GB
 */
void create_prelaunched_vm_e820(struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	uint64_t gpa_start = 0x100000000UL;
	uint64_t hpa1_hi_size, hpa2_lo_size;
	uint64_t remaining_hpa2_size = vm_config->memory.size_hpa2;
	uint32_t entry_idx = ENTRY_HPA1_HI;

	vm->e820_entries = pre_vm_e820[vm->vm_id];
	(void)memcpy_s((void *)vm->e820_entries,  E820_MAX_ENTRIES * sizeof(struct e820_entry),
		(const void *)pre_ve820_template, E820_MAX_ENTRIES * sizeof(struct e820_entry));

	/* sanitize entry for hpa1 */
	if (vm_config->memory.size > MEM_2G) {
		/* need to split hpa1 and add an entry for hpa1_hi */
		hpa1_hi_size = vm_config->memory.size - MEM_2G;
		gpa_start = add_ram_entry((vm->e820_entries + entry_idx), gpa_start, hpa1_hi_size);
		entry_idx++;
	} else {
		/* need to revise length of hpa1 entry to its actual size */
		vm->e820_entries[ENTRY_HPA1].length = vm_config->memory.size - MEM_1M;
		if ((vm_config->memory.size < MEM_2G)
				&& (remaining_hpa2_size > (MEM_2G - vm_config->memory.size))) {
			/* need to split hpa2 and add an entry for hpa2_lo */
			hpa2_lo_size = remaining_hpa2_size - (MEM_2G - vm_config->memory.size);
			gpa_start = add_ram_entry((vm->e820_entries + entry_idx), gpa_start, hpa2_lo_size);
			remaining_hpa2_size -= hpa2_lo_size;
			entry_idx++;
		}
	}

	/* check whether need an entry for remaining hpa2 */
	if (remaining_hpa2_size > 0UL) {
		gpa_start = add_ram_entry((vm->e820_entries + entry_idx), gpa_start, remaining_hpa2_size);
		entry_idx++;
	}

	vm->e820_entry_num = entry_idx;
}
