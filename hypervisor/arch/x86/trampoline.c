/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/mmu.h>
#include <per_cpu.h>
#include <asm/trampoline.h>
#include <reloc.h>
#include <asm/boot/ld_sym.h>
#include <asm/e820.h>

static uint64_t trampoline_start16_paddr;

/*
 * Because trampoline code is relocated in different way, if HV code
 * accesses trampoline using relative addressing, it needs to take
 * out the HV relocation delta
 *
 * This function is valid if:
 *  - The hpa of HV code is always higher than trampoline code
 *  - The HV code is always relocated to higher address, compared
 *    with CONFIG_HV_RAM_START
 */
static uint64_t trampoline_relo_addr(const void *addr)
{
	return (uint64_t)addr - get_hv_image_delta();
}

uint64_t read_trampoline_sym(const void *sym)
{
	uint64_t *hva = (uint64_t *)(hpa2hva(trampoline_start16_paddr) + trampoline_relo_addr(sym));
	return *hva;
}

void write_trampoline_sym(const void *sym, uint64_t val)
{
	uint64_t *hva = (uint64_t *)(hpa2hva(trampoline_start16_paddr) + trampoline_relo_addr(sym));
	*hva = val;
	clflush(hva);
}

void write_trampoline_stack_sym(uint16_t pcpu_id)
{
	uint64_t *hva, stack_sym_addr;
	hva = (uint64_t *)(hpa2hva(trampoline_start16_paddr) + trampoline_relo_addr(secondary_cpu_stack));

	stack_sym_addr = (uint64_t)&per_cpu(stack, pcpu_id)[CONFIG_STACK_SIZE - 1];
	stack_sym_addr &= ~(CPU_STACK_ALIGN - 1UL);
	*hva = stack_sym_addr;

	clflush(hva);
}

uint64_t get_trampoline_start16_paddr(void)
{
	return trampoline_start16_paddr;
}

static void update_trampoline_code_refs(uint64_t dest_pa)
{
	void *ptr;
	uint64_t val;
	uint32_t i;

	/*
	 * calculate the fixup CS:IP according to fixup target address
	 * dynamically.
	 *
	 * trampoline code starts in real mode,
	 * so the target addres is HPA
	 */
	val = dest_pa + trampoline_relo_addr(&trampoline_fixup_target);

	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_fixup_cs));
	*(uint16_t *)(ptr) = (uint16_t)((val >> 4U) & 0xFFFFU);

	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_fixup_ip));
	*(uint16_t *)(ptr) = (uint16_t)(val & 0xfU);

	/* Update temporary page tables */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&cpu_boot_page_tables_ptr));
	*(uint32_t *)(ptr) += (uint32_t)dest_pa;

	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&cpu_boot_page_tables_start));
	*(uint64_t *)(ptr) += dest_pa;

	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_pdpt_addr));
	for (i = 0U; i < 4U; i++) {
		*(uint64_t *)(ptr + sizeof(uint64_t) * i) += dest_pa;
	}

	/* update the gdt base pointer with relocated offset */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_gdt_ptr));
	*(uint64_t *)(ptr + 2) += dest_pa;

	/* update trampoline jump pointer with relocated offset */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(&trampoline_start64_fixup));
	*(uint32_t *)ptr += (uint32_t)dest_pa;

	/* update trampoline's main entry pointer */
	ptr = hpa2hva(dest_pa + trampoline_relo_addr(main_entry));
	*(uint64_t *)ptr += get_hv_image_delta();
}

uint64_t prepare_trampoline(void)
{
	uint64_t trampline_size, trampoline_pa;

	trampline_size = (uint64_t)(&ld_trampoline_end - &ld_trampoline_start);
	trampoline_pa = e820_alloc_memory(trampline_size, MEM_1M);

	pr_dbg("trampoline code: %lx trampline_size %x", trampoline_pa, trampline_size);

	/* Copy segment for AP initialization code below 1MB */
	(void)memcpy_s(hpa2hva(trampoline_pa), (size_t)trampline_size, &ld_trampoline_load,
			(size_t)trampline_size);
	update_trampoline_code_refs(trampoline_pa);

	cpu_memory_barrier();
	flush_cache_range(hpa2hva(trampoline_pa), trampline_size);

	trampoline_start16_paddr = trampoline_pa;

	return trampoline_pa;
}
