/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <reloc.h>

/* get the delta between CONFIG_HV_RAM_START and the actual load address */
uint64_t arch_get_hv_image_delta(void)
{
	uint64_t delta;

	asm volatile (
		"lla	%0, _start\n"
		"li	t1, %1\n"
		"sub	%0, %0, t1\n"
		: "=r" (delta)
		: "i" ((uint64_t)CONFIG_HV_RAM_START)
		: "t1");

	return delta;
}

#ifdef CONFIG_RELOC
void relocate(struct Elf64_Dyn *dynamic)
{
	struct Elf64_Dyn *dyn;
	struct Elf64_Rel *entry = NULL;
	uint8_t *rela_start = NULL, *rela_end = NULL;
	uint64_t rela_size = 0;
	uint64_t delta, entry_size = 0;
	uint64_t *addr;

	/* get the delta that needs to be patched */
	delta = get_hv_image_delta();
	/* Look for the descriptoin of relocation sections */
	for (dyn = (struct Elf64_Dyn *)dynamic;
		dyn->d_tag != DT_NULL; dyn++) {
		switch (dyn->d_tag) {
		case DT_RELA:
			rela_start = (uint8_t *)(dyn->d_ptr + delta);
			break;
		case DT_RELASZ:
			rela_size = dyn->d_ptr;
			break;
		case DT_RELAENT:
			entry_size = dyn->d_ptr;
			break;
		default:
			/* if no RELA/RELASZ found, both start and end will be
			 * initialized to NULL, and later while loop won't be executed
			 */
			break;
		}
	}

	/*
	 * Need to subtract the relocation delta to get the correct
	 * absolute addresses
	 */
	rela_end = rela_start + rela_size;
	while (rela_start < rela_end) {
		entry = (struct Elf64_Rel *)rela_start;
		if ((elf64_r_type(entry->r_info)) == R_RISCV_RELATIVE) {
			addr = (uint64_t *)(delta + entry->r_offset);
			*addr += (entry->r_addend + delta);
		}
		rela_start += entry_size;
	}
}
#endif /* CONFIG_RELOC */
