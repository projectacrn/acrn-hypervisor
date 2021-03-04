/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <reloc.h>
#include <x86/boot/ld_sym.h>

#ifdef CONFIG_RELOC
#define DT_NULL		0U	/* end of .dynamic section */
#define DT_RELA		7U	/* relocation table */
#define DT_RELASZ	8U	/* size of reloc table */
#define DT_RELAENT	9U	/* size of one entry */

#define R_X86_64_RELATIVE	8UL

struct Elf64_Dyn {
	uint64_t d_tag;
	uint64_t d_ptr;
};

struct Elf64_Rel {
	uint64_t r_offset;
	uint64_t r_info;
	uint64_t reserved;
};
#endif

static inline uint64_t elf64_r_type(uint64_t i)
{
	return (i & 0xffffffffUL);
}

/* get the delta between CONFIG_HV_RAM_START and the actual load address */
uint64_t get_hv_image_delta(void)
{
	uint64_t addr;

	asm volatile (" call 0f\n"
		"0: pop %%rax\n"
		"	sub $0b, %%rax\n"
		"	mov %%rax, %0\n"
		: "=m" (addr)
		:
		: "%rax");

	return addr;
}

/* get the actual Hypervisor load address (HVA) */
uint64_t get_hv_image_base(void)
{
	return (get_hv_image_delta() + CONFIG_HV_RAM_START);
}

void relocate(void)
{
#ifdef CONFIG_RELOC
	struct Elf64_Dyn *dyn;
	struct Elf64_Rel *entry = NULL;
	uint8_t *rela_start = NULL, *rela_end = NULL;
	uint64_t rela_size = 0;
	uint64_t delta, entry_size = 0;
	uint64_t trampoline_end;
	uint64_t primary_entry_end;
	uint64_t *addr;

	/* get the delta that needs to be patched */
	delta = get_hv_image_delta();
	if (delta != 0U) {

		/* Look for the descriptoin of relocation sections */
		for (dyn = (struct Elf64_Dyn *)_DYNAMIC; dyn->d_tag != DT_NULL; dyn++) {
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
				/* if no RELA/RELASZ found, both start and end will be initialized to NULL, and later while loop won't be executed */
				break;
			}
		}

		/*
		 * Need to subtract the relocation delta to get the correct
		 * absolute addresses
		 */
		trampoline_end = (uint64_t)(&ld_trampoline_end) - delta;
		primary_entry_end = (uint64_t)(&ld_entry_end) - delta;

		rela_end = rela_start + rela_size;
		while (rela_start < rela_end) {
			entry = (struct Elf64_Rel *)rela_start;
			if ((elf64_r_type(entry->r_info)) == R_X86_64_RELATIVE) {
				addr = (uint64_t *)(delta + entry->r_offset);

				/*
				 * we won't fixup any symbols from trampoline.S or cpu_primary.S
				 * for a number of reasons:
				 *
				 * - trampoline code itself takes another relocation,
				 *   so any entries for trampoline symbols can't be fixed up
				 *   through .rela sections
				 * - Linker option "-z noreloc-overflow" could force R_X86_32
				 *   to R_X86_64 in the relocation sections, which could make
				 *   the fixed up code dirty.
				 */
				if ((entry->r_offset > trampoline_end) && (entry->r_offset > primary_entry_end)) {
					*addr += delta;
				}
			}
			rela_start += entry_size;
		}
	}
#endif
}
