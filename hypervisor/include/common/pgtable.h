/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef COMMON_PGTABLE_H
#define COMMON_PGTABLE_H
#include <asm/page.h>
#include <asm/mm_common.h>

uint64_t arch_pgtl_page_paddr(uint64_t pgtle);
uint64_t arch_pgtl_large(uint64_t pgtle);

/**
 * @brief Translate a host physical address to a host virtual address.
 *
 * This function is used to translate a host physical address to a host virtual address. HPA is 1:1 mapping to HVA.
 *
 * It returns the host virtual address that corresponds to the given host physical address.
 *
 * @param[in] hpa The host physical address to be translated.
 *
 * @return The translated host virtual address
 *
 * @retval NULL if hpa == 0
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @remark This function is used after paging mode enabled.
 */
static inline void *hpa2hva(uint64_t hpa)
{
	return (void *)hpa;
}

/**
 * @brief Translate a host virtual address to a host physical address.
 *
 * This function is used to translate a host virtual address to a host physical address. HVA is 1:1 mapping to HPA.
 *
 * It returns the host physical address that corresponds to the given host virtual address.
 *
 * @param[in] va The host virtual address to be translated.
 *
 * @return The translated host physical address.
 *
 * @retval 0 if va == NULL
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @remark This function is used after paging mode enabled.
 */
static inline uint64_t hva2hpa(const void *va)
{
	return (uint64_t)va;
}

static inline uint64_t round_page_up(uint64_t addr)
{
	return (((addr + (uint64_t)PAGE_SIZE) - 1UL) & PAGE_MASK);
}

static inline uint64_t round_page_down(uint64_t addr)
{
	return (addr & PAGE_MASK);
}

static inline uint64_t pgtl3e_index(uint64_t address)
{
	return (address >> PGTL3_SHIFT) & (PTRS_PER_PGTL3E - 1UL);
}

static inline uint64_t pgtl2e_index(uint64_t address)
{
	return (address >> PGTL2_SHIFT) & (PTRS_PER_PGTL2E- 1UL);
}

static inline uint64_t pgtl1e_index(uint64_t address)
{
	return (address >> PGTL1_SHIFT) & (PTRS_PER_PGTL1E - 1UL);
}

static inline uint64_t pgtl0e_index(uint64_t address)
{
	return (address >> PGTL0_SHIFT) & (PTRS_PER_PGTL0E - 1UL);
}

static inline uint64_t *page_addr(uint64_t pgtle)
{
	return hpa2hva(arch_pgtl_page_paddr(pgtle));
}

static inline uint64_t is_pgtl_large(uint64_t pgtle)
{
        return arch_pgtl_large(pgtle);
}

/**
 * @brief Calculate the page map PGT_LVL3 table entry for a specified input address.
 *
 * The page map PGT_LVL3 table contains 512 entries, each of which points to a PGT_LVL2 page table.
 * Address has the index to the PGT_LVL3 entry. This function is used to calculate the address of PGT_LVL3 entry.
 * It is typically used during the page translation process.
 *
 * It will return a pointer to the page map PGT_LVL3 table entry.
 *
 * @param[in] pgtl3_page A pointer to a PGT_LVL3 page.
 * @param[in] addr The address value for which the page map PGT_LVL3 table entry address is to be calculated.
 *                 For hypervisor's MMU, it is the host virtual address.
 *                 For each VM's stage 2 tranlation, it is the guest physical address.
 *
 * @return A pointer to the PGT_LVL3 entry.
 *
 * @pre pgtl3_page != NULL
 *
 * @post N/A
 */
static inline uint64_t *pgtl3e_offset(uint64_t *pgtl3_page, uint64_t addr)
{
	return pgtl3_page + pgtl3e_index(addr);
}

/**
 * @brief Calculate the PGT_LVL2 page table entry for a specified input address.
 *
 * The PGT_LVL2 page table is referenced by a page map PGT_LVL3 table entry and echo entry
 * in PGT_LVL2 points to a PGT_LVL1 page table. Address has the index to the PGT_LVL2 entry. This function is used to
 * calculate the address of PGT_LVL2 entry. It is typically used during the page translation process.
 *
 * It will return a pointer to the PGT_LVL2 page table entry.
 *
 * @param[in] pgtl3e A pointer to a PGT_LVL3 page map table entry.
 * @param[in] addr The address for which the PGT_LVL2 page table entry address is to be calculated.
 *                 For hypervisor's MMU, it is the host virtual address.
 *                 For each VM's stage2 tranlation, it is the guest physical address.
 *
 * @return A pointer to the PGT_LVL2 entry.
 *
 * @pre pgtl3e != NULL
 *
 * @post N/A
 */
static inline uint64_t *pgtl2e_offset(const uint64_t *pgtl3e, uint64_t addr)
{
	return page_addr(*pgtl3e) + pgtl2e_index(addr);
}

/**
 * @brief Calculate the PGT_LVL1 page table entry for a specified input address.
 *
 * The PGT_LVL1 page table is referenced by a PGT_LVL2 page table entry and echo entry
 * points to a page table. Address has the index to the entry in PGT_LVL1 page table . This function
 * is used to calculate the address of PDE. It is typically used during the page translation process.
 *
 * It will return a pointer to the PGT_LVL1 page table entry.
 *
 * @param[in] pgtl2e A pointer to a PGT_LVL2 page table entry.
 * @param[in] addr The address for which the PGT_LVL1 page table entry address is to be calculated.
 *                 For hypervisor's MMU, it is the host virtual address.
 *                 For each VM's stage 2 translation, it is the guest physical address.
 *
 * @return A pointer to the PGT_LVL1 page table entry.
 *
 * @pre pgtl2e != NULL
 *
 * @post N/A
 */
static inline uint64_t *pgtl1e_offset(const uint64_t *pgtl2e, uint64_t addr)
{
	return page_addr(*pgtl2e) + pgtl1e_index(addr);
}

/**
 * @brief Calculate the PGT_LVL0 page table entry for a specified input address.
 *
 * The PGT_LVL0 page table entry is the entry that maps a page. This function is used to calculate
 * the address of the PGT_LVL0 entry. It is typically used during the page translation process.
 * The function is essential for managing memory access permissions and for implementing memory systems.
 *
 * It will return the address of a PGT_LVL0 page table entry.
 *
 * @param[in] pgtl1e A pointer to a PGT_LVL1 page table entry.
 * @param[in] addr The address for which the PGT_LVL1 page table entry address is to be calculated.
 *                 For hypervisor's MMU, it is the host virtual address.
 *                 For each VM's stage 2 translation, it is the guest physical address.
 *
 * @return A pointer to the PGT_LVL0 page table entry.
 *
 * @pre pgtl1e != NULL
 *
 * @post N/A
 */
static inline uint64_t *pgtl0e_offset(const uint64_t *pgtl1e, uint64_t addr)
{
	return page_addr(*pgtl1e) + pgtl0e_index(addr);
}

static inline uint64_t round_pgtl1_up(uint64_t val)
{
	return (((val + (uint64_t)PGTL1_SIZE) - 1UL) & PGTL1_MASK);
}

static inline uint64_t round_pgtl1_down(uint64_t val)
{
	return (val & PGTL1_MASK);
}

static inline uint64_t pfn2paddr(uint64_t pfn)
{
	return ((pfn >> PAGE_PFN_OFFSET) << PAGE_SHIFT);
}

static inline uint64_t paddr2pfn(uint64_t paddr)
{
	return ((paddr >> PAGE_SHIFT) << PAGE_PFN_OFFSET);
}

#endif /* COMMON_PGTABLE_H*/
