/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file pgtable.h
 *
 * @brief Address translation and page table operations
 */
#ifndef PGTABLE_H
#define PGTABLE_H

#include <x86/page.h>

#define PAGE_PRESENT		(1UL << 0U)
#define PAGE_RW			(1UL << 1U)
#define PAGE_USER		(1UL << 2U)
#define PAGE_PWT		(1UL << 3U)
#define PAGE_PCD		(1UL << 4U)
#define PAGE_ACCESSED		(1UL << 5U)
#define PAGE_DIRTY		(1UL << 6U)
#define PAGE_PSE		(1UL << 7U)
#define PAGE_GLOBAL		(1UL << 8U)
#define PAGE_PAT_LARGE		(1UL << 12U)
#define PAGE_NX			(1UL << 63U)

#define PAGE_CACHE_MASK		(PAGE_PCD | PAGE_PWT)
#define PAGE_CACHE_WB		0UL
#define PAGE_CACHE_WT		PAGE_PWT
#define PAGE_CACHE_UC_MINUS	PAGE_PCD
#define PAGE_CACHE_UC		(PAGE_PCD | PAGE_PWT)

/**
 * @defgroup ept_mem_access_right EPT Memory Access Right
 *
 * This is a group that includes EPT Memory Access Right Definitions.
 *
 * @{
 */

/**
 * @brief EPT memory access right is read-only.
 */
#define EPT_RD			(1UL << 0U)

/**
 * @brief EPT memory access right is read/write.
 */
#define EPT_WR			(1UL << 1U)

/**
 * @brief EPT memory access right is executable.
 */
#define EPT_EXE			(1UL << 2U)

/**
 * @brief EPT memory access right is read/write and executable.
 */
#define EPT_RWX			(EPT_RD | EPT_WR | EPT_EXE)

/**
 * @}
 */
/* End of ept_mem_access_right */

/**
 * @defgroup ept_mem_type EPT Memory Type
 *
 * This is a group that includes EPT Memory Type Definitions.
 *
 * @{
 */

/**
 * @brief EPT memory type is specified in bits 5:3 of the EPT paging-structure entry.
 */
#define EPT_MT_SHIFT		3U

/**
 * @brief EPT memory type is uncacheable.
 */
#define EPT_UNCACHED		(0UL << EPT_MT_SHIFT)

/**
 * @brief EPT memory type is write combining.
 */
#define EPT_WC			(1UL << EPT_MT_SHIFT)

/**
 * @brief EPT memory type is write through.
 */
#define EPT_WT			(4UL << EPT_MT_SHIFT)

/**
 * @brief EPT memory type is write protected.
 */
#define EPT_WP			(5UL << EPT_MT_SHIFT)

/**
 * @brief EPT memory type is write back.
 */
#define EPT_WB			(6UL << EPT_MT_SHIFT)

/**
 * @}
 */
/* End of ept_mem_type */

#define EPT_MT_MASK		(7UL << EPT_MT_SHIFT)
#define EPT_VE			(1UL << 63U)
/* EPT leaf entry bits (bit 52 - bit 63) should be maksed  when calculate PFN */
#define EPT_PFN_HIGH_MASK	0xFFF0000000000000UL

#define PML4E_SHIFT		39U
#define PTRS_PER_PML4E		512UL
#define PML4E_SIZE		(1UL << PML4E_SHIFT)
#define PML4E_MASK		(~(PML4E_SIZE - 1UL))

#define PDPTE_SHIFT		30U
#define PTRS_PER_PDPTE		512UL
#define PDPTE_SIZE		(1UL << PDPTE_SHIFT)
#define PDPTE_MASK		(~(PDPTE_SIZE - 1UL))

#define PDE_SHIFT		21U
#define PTRS_PER_PDE		512UL
#define PDE_SIZE		(1UL << PDE_SHIFT)
#define PDE_MASK		(~(PDE_SIZE - 1UL))

#define PTE_SHIFT		12U
#define PTRS_PER_PTE		512UL
#define PTE_SIZE		(1UL << PTE_SHIFT)
#define PTE_MASK		(~(PTE_SIZE - 1UL))

/* TODO: PAGE_MASK & PHYSICAL_MASK */
#define PML4E_PFN_MASK		0x0000FFFFFFFFF000UL
#define PDPTE_PFN_MASK		0x0000FFFFFFFFF000UL
#define PDE_PFN_MASK		0x0000FFFFFFFFF000UL
/**
 * @brief Address space translation
 *
 * @addtogroup acrn_mem ACRN Memory Management
 * @{
 */
/* hpa <--> hva, now it is 1:1 mapping */
/**
 * @brief Translate host-physical address to host-virtual address
 *
 * @param[in] x The specified host-physical address
 *
 * @return The translated host-virtual address
 */
static inline void *hpa2hva_early(uint64_t x)
{
	return (void *)x;
}
/**
 * @brief Translate host-virtual address to host-physical address
 *
 * @param[in] x The specified host-virtual address
 *
 * @return The translated host-physical address
 */
static inline uint64_t hva2hpa_early(void *x)
{
	return (uint64_t)x;
}

/**
 * @brief Translate host-physical address to host-virtual address
 *
 * @param[in] x The specified host-physical address
 *
 * @return The translated host-virtual address
 */
static inline void *hpa2hva(uint64_t x)
{
	return (void *)x;
}
/**
 * @brief Translate host-virtual address to host-physical address
 *
 * @param[in] x The specified host-virtual address
 *
 * @return The translated host-physical address
 */
static inline uint64_t hva2hpa(const void *x)
{
	return (uint64_t)x;
}

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
	return hpa2hva(pml4e & PML4E_PFN_MASK);
}

static inline uint64_t *pdpte_page_vaddr(uint64_t pdpte)
{
	return hpa2hva(pdpte & PDPTE_PFN_MASK);
}

static inline uint64_t *pde_page_vaddr(uint64_t pde)
{
	return hpa2hva(pde & PDE_PFN_MASK);
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
static inline void set_pgentry(uint64_t *ptep, uint64_t pte, const struct memory_ops *mem_ops)
{
	*ptep = pte;
	mem_ops->clflush_pagewalk(ptep);
}

static inline uint64_t pde_large(uint64_t pde)
{
	return pde & PAGE_PSE;
}

static inline uint64_t pdpte_large(uint64_t pdpte)
{
	return pdpte & PAGE_PSE;
}

/**
 *@pre (pml4_page != NULL) && (pg_size != NULL)
 */
const uint64_t *lookup_address(uint64_t *pml4_page, uint64_t addr,
		uint64_t *pg_size, const struct memory_ops *mem_ops);

/**
 * @}
 */
#endif /* PGTABLE_H */
