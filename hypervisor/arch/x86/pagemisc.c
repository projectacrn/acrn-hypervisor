/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <util.h>
#include <acrn_hv_defs.h>
#include <asm/page.h>
#include <pgtable.h>
#include <mmu.h>
#include <logmsg.h>

#if CONFIG_SERIAL_8250_PCI
static uint8_t uart_pde_page[PAGE_SIZE]__aligned(PAGE_SIZE);
static uint8_t uart_pdpte_page[PAGE_SIZE]__aligned(PAGE_SIZE);

void early_pgtable_map_uart(uint64_t addr)
{
	uint64_t *pml4e, *pdpte, *pde;
	uint64_t value;

	CPU_CR_READ(cr3, &value);
	/*assumpiton for map high mmio in early pagetable is that it is only used for
	  2MB page since 1G page may not available when memory width is 39bit */
	pml4e = pgtl3e_offset((uint64_t *)value, addr);
	/* address is above 512G */
	if(!(*pml4e & PAGE_PRESENT)) {
		*pml4e = hva2hpa_early(uart_pdpte_page) + (PAGE_PRESENT|PAGE_RW);
	}
	pdpte = pgtl2e_offset(pml4e, addr);
	if(!(*pdpte & PAGE_PRESENT)) {
		*(pdpte) = hva2hpa_early(uart_pde_page) + (PAGE_PRESENT|PAGE_RW);
		pde = pgtl1e_offset(pdpte, addr);
		*pde =  (addr & PGTL1_MASK) + (PAGE_PRESENT|PAGE_RW|PAGE_PSE);
	} else if(!(*pdpte & PAGE_PSE)) {
		pde = pgtl1e_offset(pdpte, addr);
		if(!(*pde & PAGE_PRESENT)) {
			*pde = (addr & PGTL1_MASK) + (PAGE_PRESENT|PAGE_RW|PAGE_PSE);
		}
	} else {
		/* no process */
	}
}
#endif

/**
 * @brief Create a root page table for Secure World.
 *
 * This function initializes a new root page table for Secure World. It is intended to be used during the initialization
 * phase of Trusty, setting up isolated memory regions for secure execution. Secure world can access Normal World's
 * memory, but Normal World cannot access Secure World's memory. The PML4T/PDPT for Secure World are separated from
 * Normal World. PDT/PT are shared in both Secure World's EPT and Normal World's EPT. So this function copies the PDPTEs
 * from the Normal World to the Secure World.
 *
 * - It creates a new root page table and every entries are initialized to point to the sanitized page by default.
 * - The access right specified by prot_clr is cleared for Secure World PDPTEs.
 * - Finally, the function returns the new root page table pointer.
 *
 * @param[in] table A pointer to the struct pgtable containing the information of the specified memory operations.
 * @param[in] nworld_pml4_page A pointer to pml4 table hierarchy in Normal World.
 * @param[in] prot_table_present Mask indicating the page referenced is present.
 * @param[in] prot_clr Bit positions representing the specified properties which need to be cleared.
 *
 * @return A pointer to the newly created root page table for Secure World.
 *
 * @pre table != NULL
 * @pre nworld_pml4_page != NULL
 *
 * @post N/A
 */
void *pgtable_create_trusty_root(const struct pgtable *table,
	void *nworld_pml4_page, uint64_t prot_table_present, uint64_t prot_clr)
{
	uint16_t i;
	uint64_t pdpte, *dest_pdpte_p, *src_pdpte_p;
	uint64_t nworld_pml4e, sworld_pml4e;
	void *sub_table_addr, *pml4_base;

	/* Copy PDPT entries from Normal world to Secure world
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 * The PML4/PDPT for Secure world are separated from
	 * Normal World. PD/PT are shared in both Secure world's EPT
	 * and Normal World's EPT
	 */
	pml4_base = pgtable_create_root(table);

	/* The trusty memory is remapped to guest physical address
	 * of gpa_rebased to gpa_rebased + size
	 */
	sub_table_addr = alloc_page(table->pool);
	sworld_pml4e = hva2hpa(sub_table_addr) | prot_table_present;
	*(uint64_t *)pml4_base = sworld_pml4e;
	table->flush_cache_pagewalk(pml4_base);
	nworld_pml4e = get_pgentry((uint64_t *)nworld_pml4_page);

	/*
	 * copy PTPDEs from normal world EPT to secure world EPT,
	 * and remove execute access attribute in these entries
	 */
	dest_pdpte_p = page_addr(sworld_pml4e);
	src_pdpte_p = page_addr(nworld_pml4e);
	for (i = 0U; i < (uint16_t)(PTRS_PER_PGTL2E - 1UL); i++) {
		pdpte = get_pgentry(src_pdpte_p);
		if ((pdpte & prot_table_present) != 0UL) {
			pdpte &= ~prot_clr;
			*dest_pdpte_p = pdpte;
			table->flush_cache_pagewalk(dest_pdpte_p);
		}
		src_pdpte_p++;
		dest_pdpte_p++;
	}

	return pml4_base;
}
