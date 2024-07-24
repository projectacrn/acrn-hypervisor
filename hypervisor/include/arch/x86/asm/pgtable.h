/*
 * Copyright (C) 2018-2024 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PGTABLE_H
#define PGTABLE_H

#include <asm/page.h>

/**
 * @addtogroup hwmgmt_page
 *
 * @{
 */

/**
 * @file
 * @brief All APIs to support page table management
 *
 * This file defines macros, structures, declarations and functions related for managing page tables.
 *
 */

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

#define PAGE_ATTR_USER		(PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_NX)

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
 * @brief Ignore PAT memory type.
 */
#define EPT_IGNORE_PAT		(1UL << 6U)

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

#define EPT_ENTRY_PFN_MASK	((~EPT_PFN_HIGH_MASK) & PAGE_MASK)

/**
 * @brief Page tables level in IA32 paging mode
 *
 * 4-level paging in IA32 mode may map linear addresses to 4-KByte pages, 2-MByte pages, or 1-GByte pages. The 4 levels
 * are PML4, PDPT, PD, and PT. The value to present each level is fixed.
 */
enum _page_table_level {
	IA32E_PML4 = 0,     /**< The Page-Map-Level-4(PML4) level in the page tables.
			      *  The value is fixed to 0. */
	IA32E_PDPT = 1,     /**< The Page-Directory-Pointer-Table(PDPT) level in the page tables. */
	IA32E_PD = 2,       /**< The Page-Directory(PD) level in the page tables. */
	IA32E_PT = 3,       /**< The Page-Table(PT) level in the page tables. */
};

/**
 * @brief Data structure that contains the related operations and properties of page table.
 *
 * This structure is used to add/modify/delete page table.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct pgtable {
	/**
	 * @brief Default memory access rights.
	 *
	 * A linear address can be translated to a physical address by the page tables. The translation is controlled by
	 * the memory access rights, as defined by the architecture's memory system design. The default memory access
	 * rights can be used to set the memory access rights for a page table entry when the page table is created.
	 */
	uint64_t default_access_right;
	/**
	 * @brief Mask to check if the page referenced by entry is present.
	 *
	 * The presence of a page is indicated by specific bits in the entry, as defined by the architecture's memory
	 * system design. For example, in ept table entry it's indicated by bit0|bit1|bit2, and in mmu table entry it's
	 * indicated by bit 0.
	 */
	uint64_t pgentry_present_mask;
	struct page_pool *pool; /**< Pointer to the page pool used for managing pages. */
	/**
	 * @brief Function to check if large pages are supported.
	 *
	 * This function is used to check if large pages are supported for a specific page table level and memory access
	 * rights.
	 */
	bool (*large_page_support)(enum _page_table_level level, uint64_t prot);
	void (*clflush_pagewalk)(const void *p); /**< Function to flush a page table entry from the cache. */
	void (*tweak_exe_right)(uint64_t *entry); /**< Function to tweak execution rights for an entry. */
	void (*recover_exe_right)(uint64_t *entry); /**< Function to recover execution rights for an entry. */
};

/**
 * @brief Check whether the page referenced by the specified paging-structure entry is present or not.
 *
 * This function is used to check if the page referenced is present. A paging-structure entry references a page. The
 * presence of a page is indicated by specific bits in the entry, as defined by the architecture's memory system design.
 * For example, in ept table entry it's indicated by bit0|bit1|bit2, and in mmu table entry it's indicated by bit 0.
 *
 * This function checks whether the page referenced exists based on specific bits.
 *
 * @param[in] table A pointer to the structure pgtable which provides the mask to check whether page referenced is
 *                  present or not.
 * @param[in] pte The paging-structure entry to check.
 *
 * @return A boolean value indicating if the page referenced by the specified paging-structure entry is present
 *
 * @retval true Indicates the page referenced is present.
 * @retval false Indicates the page referenced is not present.
 *
 * @pre table != NULL
 *
 * @post N/A
 */
static inline bool pgentry_present(const struct pgtable *table, uint64_t pte)
{
	return ((table->pgentry_present_mask & (pte)) != 0UL);
}

/**
 * @brief Translate a host physical address to a host virtual address before paging mode enabled.
 *
 * This function is used to translate a host physical address to a host virtual address before paging mode enabled. HPA
 * is 1:1 mapping to HVA.
 *
 * It returns the host virtual address that corresponds to the given host physical address.
 *
 * @param[in] x The host physical address
 *
 * @return The translated host virtual address
 *
 * @retval NULL if x == 0
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @remark This function is used before paging mode enabled.
 */
static inline void *hpa2hva_early(uint64_t x)
{
	return (void *)x;
}

/**
 * @brief Translate a host virtual address to a host physical address before paging mode enabled.
 *
 * This function is used to translate a host virtual address to a host physical address before paging mode enabled. HVA
 * is 1:1 mapping to HPA.
 *
 * It returns the host physical address that corresponds to the given host virtual address.
 *
 * @param[in] x The host virtual address to be translated
 *
 * @return The translated host physical address
 *
 * @retval 0 if x == NULL
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @remark This function is used before paging mode enabled.
 */
static inline uint64_t hva2hpa_early(void *x)
{
	return (uint64_t)x;
}

/**
 * @brief Translate a host physical address to a host virtual address.
 *
 * This function is used to translate a host physical address to a host virtual address. HPA is 1:1 mapping to HVA.
 *
 * It returns the host virtual address that corresponds to the given host physical address.
 *
 * @param[in] x The host physical address to be translated.
 *
 * @return The translated host virtual address
 *
 * @retval NULL if x == 0
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @remark This function is used after paging mode enabled.
 */
static inline void *hpa2hva(uint64_t x)
{
	return (void *)x;
}

/**
 * @brief Translate a host virtual address to a host physical address.
 *
 * This function is used to translate a host virtual address to a host physical address. HVA is 1:1 mapping to HPA.
 *
 * It returns the host physical address that corresponds to the given host virtual address.
 *
 * @param[in] x The host virtual address to be translated.
 *
 * @return The translated host physical address.
 *
 * @retval 0 if x == NULL
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @remark This function is used after paging mode enabled.
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

/**
 * @brief Calculate the page map level-4 table entry(PML4E) for a specified input address.
 *
 * The page map level-4 table(PML4T) contains 512 entries, each of which points to a page directory pointer table(PDPT).
 * Address has the index to the PML4E in PML4T. This function is used to calculate the address of PML4E. It is typically
 * used during the page translation process.
 *
 * It will return a pointer to the page map level-4 table entry(PML4E).
 *
 * @param[in] pml4_page A pointer to a page map level-4 table(PML4T) page.
 * @param[in] addr The address value for which the page map level-4 table entry(PML4E) address is to be calculated.
 *                 For hypervisor's MMU, it is the host virtual address.
 *                 For each VM's EPT, it is the guest physical address.
 *
 * @return A pointer to the PML4E.
 *
 * @pre pml4_page != NULL
 *
 * @post N/A
 */
static inline uint64_t *pml4e_offset(uint64_t *pml4_page, uint64_t addr)
{
	return pml4_page + pml4e_index(addr);
}

/**
 * @brief Calculate the page directory pointer table entry(PDPTE) for a specified input address.
 *
 * The page directory pointer table(PDPT) is referenced by a page map level-4 table entry(PML4E) and echo entry(PDPTE)
 * in PDPT points to a page directory table(PDT). Address has the index to the PDPTE in PDPT. This function is used to
 * calculate the address of PDPTE. It is typically used during the page translation process.
 *
 * It will return a pointer to the page directory pointer table entry(PDPTE).
 *
 * @param[in] pml4e A pointer to a page map level-4 table entry(PML4E).
 * @param[in] addr The address for which the page directory pointer table entry(PDPTE) address is to be calculated.
 *                 For hypervisor's MMU, it is the host virtual address.
 *                 For each VM's EPT, it is the guest physical address.
 *
 * @return A pointer to the PDPTE.
 *
 * @pre pml4e != NULL
 *
 * @post N/A
 */
static inline uint64_t *pdpte_offset(const uint64_t *pml4e, uint64_t addr)
{
	return pml4e_page_vaddr(*pml4e) + pdpte_index(addr);
}

/**
 * @brief Calculate the page directory table entry(PDE) for a specified input address.
 *
 * The page directory table(PDT) is referenced by a page directory pointer table entry(PDPTE) and echo entry(PDE) in PDT
 * points to a page table(PT). Address has the index to the PDE in PDT. This function is used to calculate the address
 * of PDE. It is typically used during the page translation process.
 *
 * It will return a pointer to the page directory table entry(PDE).
 *
 * @param[in] pdpte A pointer to a page directory pointer table entry(PDPTE).
 * @param[in] addr The address for which the page directory table entry(PDE) address is to be calculated.
 *                 For hypervisor's MMU, it is the host virtual address.
 *                 For each VM's EPT, it is the guest physical address.
 *
 * @return A pointer to the PDE.
 *
 * @pre pdpte != NULL
 *
 * @post N/A
 */
static inline uint64_t *pde_offset(const uint64_t *pdpte, uint64_t addr)
{
	return pdpte_page_vaddr(*pdpte) + pde_index(addr);
}

/**
 * @brief Calculate the page table entry(PTE) for a specified input address.
 *
 * The page table entry(PTE) is the entry that maps a page. This function is used to calculate the address of the PTE.
 * It is typically used during the page translation process. The function is essential for managing memory access
 * permissions and for implementing memory systems.
 *
 * It will return the address of a page table entry(PTE).
 *
 * @param[in] pde A pointer to a page directory entry(PDE).
 * @param[in] addr The address for which the page table entry(PTE) address is to be calculated.
 *                 For hypervisor's MMU, it is the host virtual address.
 *                 For each VM's EPT, it is the guest physical address.
 *
 * @return A pointer to the page table entry(PTE).
 *
 * @pre pde != NULL
 *
 * @post N/A
 */
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
static inline void set_pgentry(uint64_t *ptep, uint64_t pte, const struct pgtable *table)
{
	*ptep = pte;
	table->clflush_pagewalk(ptep);
}

/**
 * @brief Check whether the PS flag of the specified page directory table entry(PDE) is 1 or not.
 *
 * PS(Page Size) flag in PDE indicates whether maps a 2-MByte page or references a page table. This function checks this
 * flag. This function is typically used in the context of setting up or modifying page tables where it's necessary to
 * distinguish between large and regular page mappings.
 *
 * It returns the value that bit 7 is 1 if the specified PDE maps a 2-MByte page, or 0 if references a page table.
 *
 * @param[in] pde The page directory table entry(PDE) to check.
 *
 * @return The value of PS flag in the PDE.
 *
 * @retval PAGE_PSE indicating mapping to a 2-MByte page.
 * @retval 0 indicating reference to a page table.
 *
 * @pre N/A
 *
 * @post N/A
 */
static inline uint64_t pde_large(uint64_t pde)
{
	return pde & PAGE_PSE;
}

/**
 * @brief Check whether the PS flag of the specified page directory pointer table entry(PDPTE) is 1 or not.
 *
 * PS(Page Size) flag in PDPTE indicates whether maps a 1-GByte page or references a page directory table. This function
 * checks this flag. This function is typically used in the context of setting up or modifying page tables where it's
 * necessary to distinguish between large and regular page mappings.
 *
 * It returns the value that bit 7 is 1 if the specified PDPTE maps a 1-GByte page, and 0 if references a page table.
 *
 * @param[in] pdpte The page directory pointer table entry(PDPTE) to check.
 *
 * @return The value of PS flag in the PDPTE.
 *
 * @retval PAGE_PSE indicating mapping to a 1-GByte page.
 * @retval 0 indicating reference to a page directory table.
 *
 * @pre N/A
 *
 * @post N/A
 */
static inline uint64_t pdpte_large(uint64_t pdpte)
{
	return pdpte & PAGE_PSE;
}

void init_sanitized_page(uint64_t *sanitized_page, uint64_t hpa);

void *pgtable_create_root(const struct pgtable *table);
void *pgtable_create_trusty_root(const struct pgtable *table,
	void *nworld_pml4_page, uint64_t prot_table_present, uint64_t prot_clr);
/**
 *@pre (pml4_page != NULL) && (pg_size != NULL)
 */
const uint64_t *pgtable_lookup_entry(uint64_t *pml4_page, uint64_t addr,
		uint64_t *pg_size, const struct pgtable *table);

void pgtable_add_map(uint64_t *pml4_page, uint64_t paddr_base,
		uint64_t vaddr_base, uint64_t size,
		uint64_t prot, const struct pgtable *table);
void pgtable_modify_or_del_map(uint64_t *pml4_page, uint64_t vaddr_base,
		uint64_t size, uint64_t prot_set, uint64_t prot_clr,
		const struct pgtable *table, uint32_t type);
#endif /* PGTABLE_H */

/**
 * @}
 */