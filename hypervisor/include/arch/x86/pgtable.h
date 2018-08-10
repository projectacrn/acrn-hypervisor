/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PGTABLE_H
#define PGTABLE_H

#include <pgtable_types.h>

/* hpa <--> hva, now it is 1:1 mapping */
#define HPA2HVA(x) ((void *)(x))
#define HVA2HPA(x) ((uint64_t)(x))

static inline uint64_t pml4e_index(uint64_t address)
{
	return (address >> PML4E_SHIFT) & (PTRS_PER_PML4E - 1UL);
}

static inline uint64_t pdpte_index(uint64_t address)
{
	return (address >> PDPTE_SHIFT) & (PTRS_PER_PDPTE - 1UL);
}

static inline uint64_t pde_index(uint64_t address)
{
	return (address >> PDE_SHIFT) & (PTRS_PER_PDE - 1UL);
}

static inline uint64_t pte_index(uint64_t address)
{
	return (address >> PTE_SHIFT) & (PTRS_PER_PTE - 1UL);
}

static inline uint64_t *pml4e_page_vaddr(uint64_t pml4e)
{
	return HPA2HVA(pml4e & PML4E_PFN_MASK);
}

static inline uint64_t *pdpte_page_vaddr(uint64_t pdpte)
{
	return HPA2HVA(pdpte & PDPTE_PFN_MASK);
}

static inline uint64_t *pde_page_vaddr(uint64_t pde)
{
	return HPA2HVA(pde & PDE_PFN_MASK);
}

static inline uint64_t *pml4e_offset(uint64_t *pml4_page, uint64_t addr)
{
	return pml4_page + pml4e_index(addr);
}

static inline uint64_t *pdpte_offset(const uint64_t *pml4e, uint64_t addr)
{
	return pml4e_page_vaddr(*pml4e) + pdpte_index(addr);
}

static inline uint64_t *pde_offset(const uint64_t *pdpte, uint64_t addr)
{
	return pdpte_page_vaddr(*pdpte) + pde_index(addr);
}

static inline uint64_t *pte_offset(const uint64_t *pde, uint64_t addr)
{
	return pde_page_vaddr(*pde) + pte_index(addr);
}

/*
 * pgentry may means pml4e/pdpte/pde/pte
 */
static inline uint64_t get_pgentry(const uint64_t *pte)
{
	return *pte;
}

/*
 * pgentry may means pml4e/pdpte/pde/pte
 */
static inline void set_pgentry(uint64_t *ptep, uint64_t pte)
{
	*ptep = pte;
}

static inline uint64_t pde_large(uint64_t pde)
{
	return pde & PAGE_PSE;
}

static inline uint64_t pdpte_large(uint64_t pdpte)
{
	return pdpte & PAGE_PSE;
}

static inline uint64_t pgentry_present(enum _page_table_type ptt, uint64_t pte)
{
	return (ptt == PTT_HOST) ? (pte & PAGE_PRESENT) : (pte & EPT_RWX);
}

#endif /* PGTABLE_H */
