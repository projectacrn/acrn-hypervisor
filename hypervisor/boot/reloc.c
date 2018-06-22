/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <reloc.h>

struct Elf64_Dyn {
	uint64_t d_tag;
	uint64_t d_ptr;
};

#define DT_NULL		0	/* end of .dynamic section */
#define DT_RELA		7	/* relocation table */
#define DT_RELASZ	8	/* size of reloc table */
#define DT_RELAENT	9	/* size of one entry */

struct Elf64_Rel {
	uint64_t r_offset;
	uint64_t r_info;
	uint64_t reserved;
};

#define ELF64_R_TYPE(i)		((i) & 0xffffffff)
#define R_X86_64_RELATIVE	8

/* get the delta between CONFIG_RAM_START and the actual load address */
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

/* get the actual Hypervisor load address */
uint64_t get_hv_image_base(void)
{
	return (get_hv_image_delta() + CONFIG_RAM_START);
}

/*
 * Because trampoline code is relocated in different way, if HV code
 * accesses trampoline using relative addressing, it needs to take
 * out the HV relocation delta
 *
 * This function is valid if:
 *  - The hpa of HV code is always higher than trampoline code
 *  - The HV code is always relocated to higher address, compared
 *    with CONFIG_RAM_START
 */
uint64_t trampoline_relo_addr(void *addr)
{
	return (uint64_t)addr - get_hv_image_delta();
}

void _relocate(void)
{
	struct Elf64_Dyn *dyn;
	struct Elf64_Rel *start = NULL, *end = NULL;
	uint64_t delta, size = 0;
	uint64_t trampoline_end;
	uint64_t primary_32_start, primary_32_end;
	uint64_t *addr;

	/* get the delta that needs to be patched */
	delta = get_hv_image_delta();
	if (delta == 0U)
		return;

	/* Look for the descriptoin of relocation sections */
	for (dyn = (struct Elf64_Dyn *)_DYNAMIC; dyn->d_tag != DT_NULL; dyn++) {
		switch (dyn->d_tag) {
		case DT_RELA:
			start = (struct Elf64_Rel *)(dyn->d_ptr + delta);
			break;
		case DT_RELASZ:
			end = (struct Elf64_Rel *)start + dyn->d_ptr;
			break;
		case DT_RELAENT:
			size = dyn->d_ptr;
			break;
		}
	}

	/* Sanity check */
	if ((start == NULL) || (size == 0U))
		return;

	/*
	 * Need to subtract the relocation delta to get the correct
	 * absolute addresses
	 */
	trampoline_end = (uint64_t)_ld_trampoline_end - delta;
	primary_32_start = (uint64_t)cpu_primary_start_32 - delta;
	primary_32_end = (uint64_t)cpu_primary_start_64 - delta;

	while (start < end) {
		if ((ELF64_R_TYPE(start->r_info)) == R_X86_64_RELATIVE) {
			addr = (uint64_t *)(delta + start->r_offset);

			/*
			 * we won't fixup any trampoline.S and cpu_primary.S here
			 * for a number of reasons:
			 *
			 * - trampoline code itself takes another relocation,
			 *   so any entries for trampoline symbols can't be fixed up
			 *   through .rela sections
			 * - In cpu_primary.S, the 32 bits code doesn't need relocation
			 * - Linker option "-z noreloc-overflow" could force R_X86_32
			 *   to R_X86_64 in the relocation sections, which could make
			 *   the fixed up code dirty. Even if relocation for 32 bits
			 *   is needed in the future, it's recommended to do it
			 *   explicitly in the assembly code to avoid confusion.
			 */
			if ((start->r_offset > trampoline_end) &&
					((start->r_offset < primary_32_start) ||
					(start->r_offset > primary_32_end))) {
				*addr += delta;
			}
		}
		start = (struct Elf64_Rel *)((char *)start + size);
	}
}
