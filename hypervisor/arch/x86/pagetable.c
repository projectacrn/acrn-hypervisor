/*
 * Copyright (C) 2018-2024 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <util.h>
#include <acrn_hv_defs.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <logmsg.h>

/**
 * @addtogroup hwmgmt_page
 *
 * @{
 */

/**
 * @file
 * @brief Implementation page table management.
 *
 * This file implements the external APIs to establish, modify, delete, or look for the mapping information. It also
 * defines some helper functions to implement the features that are commonly used in this file.
 *
 */

#define DBG_LEVEL_MMU	6U

/**
 * @brief Host physical address of the sanitized page.
 *
 * The sanitized page is used to mitigate l1tf. This variable is used to store the host physical address of the
 * sanitized page.
 */
static uint64_t sanitized_page_hpa;

static void sanitize_pte_entry(uint64_t *ptep, const struct pgtable *table)
{
	set_pgentry(ptep, sanitized_page_hpa, table);
}

static void sanitize_pte(uint64_t *pt_page, const struct pgtable *table)
{
	uint64_t i;
	for (i = 0UL; i < PTRS_PER_PTE; i++) {
		sanitize_pte_entry(pt_page + i, table);
	}
}

/**
 * @brief Initializes a sanitized page.
 *
 * This function is responsible for initializing a sanitized page. It sets the page table entries in this sanitized page
 * to point to the host physical address of the sanitized page itself.
 *
 * The static variable 'sanitized_page_hpa' will be set and the `sanitized_page` will be initialized.
 *
 * @param[out] sanitized_page The page to be sanitized.
 * @param[in] hpa The host physical address that the page table entries in the sanitized page will point to.
 *
 * @return None
 *
 * @pre sanitized_page != NULL
 * @pre ((uint64_t)sanitized_page & (PAGE_SIZE - 1)) == 0x0U
 * @pre hpa != 0U
 * @pre (hpa & (PAGE_SIZE - 1)) == 0x0U
 *
 * @post N/A
 */
void init_sanitized_page(uint64_t *sanitized_page, uint64_t hpa)
{
	uint64_t i;

	sanitized_page_hpa = hpa;
	/* set ptep in sanitized_page point to itself */
	for (i = 0UL; i < PTRS_PER_PTE; i++) {
		*(sanitized_page + i) = sanitized_page_hpa;
	}
}

static void try_to_free_pgtable_page(const struct pgtable *table,
			uint64_t *pde, uint64_t *pt_page, uint32_t type)
{
	if (type == MR_DEL) {
		uint64_t index;

		for (index = 0UL; index < PTRS_PER_PTE; index++) {
			uint64_t *pte = pt_page + index;
			if (pgentry_present(table, (*pte))) {
				break;
			}
		}

		if (index == PTRS_PER_PTE) {
			free_page(table->pool, (void *)pt_page);
			sanitize_pte_entry(pde, table);
		}
	}
}

/*
 * Split a large page table into next level page table.
 *
 * @pre: level could only IA32E_PDPT or IA32E_PD
 */
static void split_large_page(uint64_t *pte, enum _page_table_level level,
		__unused uint64_t vaddr, const struct pgtable *table)
{
	uint64_t *pbase;
	uint64_t ref_paddr, paddr, paddrinc;
	uint64_t i, ref_prot;

	switch (level) {
	case IA32E_PDPT:
		ref_paddr = (*pte) & PDPTE_PFN_MASK;
		paddrinc = PDE_SIZE;
		ref_prot = (*pte) & ~PDPTE_PFN_MASK;
		break;
	default:	/* IA32E_PD */
		ref_paddr = (*pte) & PDE_PFN_MASK;
		paddrinc = PTE_SIZE;
		ref_prot = (*pte) & ~PDE_PFN_MASK;
		ref_prot &= ~PAGE_PSE;
		table->recover_exe_right(&ref_prot);
		break;
	}

	pbase = (uint64_t *)alloc_page(table->pool);
	dev_dbg(DBG_LEVEL_MMU, "%s, paddr: 0x%lx, pbase: 0x%lx\n", __func__, ref_paddr, pbase);

	paddr = ref_paddr;
	for (i = 0UL; i < PTRS_PER_PTE; i++) {
		set_pgentry(pbase + i, paddr | ref_prot, table);
		paddr += paddrinc;
	}

	ref_prot = table->default_access_right;
	set_pgentry(pte, hva2hpa((void *)pbase) | ref_prot, table);

	/* TODO: flush the TLB */
}

static inline void local_modify_or_del_pte(uint64_t *pte,
		uint64_t prot_set, uint64_t prot_clr, uint32_t type, const struct pgtable *table)
{
	if (type == MR_MODIFY) {
		uint64_t new_pte = *pte;
		new_pte &= ~prot_clr;
		new_pte |= prot_set;
		set_pgentry(pte, new_pte, table);
	} else {
		sanitize_pte_entry(pte, table);
	}
}

/*
 * pgentry may means pml4e/pdpte/pde
 */
static inline void construct_pgentry(uint64_t *pde, void *pd_page, uint64_t prot, const struct pgtable *table)
{
	sanitize_pte((uint64_t *)pd_page, table);

	set_pgentry(pde, hva2hpa(pd_page) | prot, table);
}

/*
 * In PT level,
 * type: MR_MODIFY
 * modify [vaddr_start, vaddr_end) memory type or page access right.
 * type: MR_DEL
 * delete [vaddr_start, vaddr_end) MT PT mapping
 */
static void modify_or_del_pte(uint64_t *pde, uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot_set, uint64_t prot_clr, const struct pgtable *table, uint32_t type)
{
	uint64_t *pt_page = pde_page_vaddr(*pde);
	uint64_t vaddr = vaddr_start;
	uint64_t index = pte_index(vaddr);

	dev_dbg(DBG_LEVEL_MMU, "%s, vaddr: [0x%lx - 0x%lx]\n", __func__, vaddr, vaddr_end);
	for (; index < PTRS_PER_PTE; index++) {
		uint64_t *pte = pt_page + index;

		if (!pgentry_present(table, (*pte))) {
			/*suppress warning message for low memory (< 1MBytes),as service VM
			 * will update MTTR attributes for this region by default whether it
			 * is present or not.
			 */
			if ((type == MR_MODIFY) && (vaddr >= MEM_1M)) {
				pr_warn("%s, vaddr: 0x%lx pte is not present.\n", __func__, vaddr);
			}
		} else {
			local_modify_or_del_pte(pte, prot_set, prot_clr, type, table);
		}

		vaddr += PTE_SIZE;
		if (vaddr >= vaddr_end) {
			break;
		}
	}

	try_to_free_pgtable_page(table, pde, pt_page, type);
}

/*
 * In PD level,
 * type: MR_MODIFY
 * modify [vaddr_start, vaddr_end) memory type or page access right.
 * type: MR_DEL
 * delete [vaddr_start, vaddr_end) MT PT mapping
 */
static void modify_or_del_pde(uint64_t *pdpte, uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot_set, uint64_t prot_clr, const struct pgtable *table, uint32_t type)
{
	uint64_t *pd_page = pdpte_page_vaddr(*pdpte);
	uint64_t vaddr = vaddr_start;
	uint64_t index = pde_index(vaddr);

	dev_dbg(DBG_LEVEL_MMU, "%s, vaddr: [0x%lx - 0x%lx]\n", __func__, vaddr, vaddr_end);
	for (; index < PTRS_PER_PDE; index++) {
		uint64_t *pde = pd_page + index;
		uint64_t vaddr_next = (vaddr & PDE_MASK) + PDE_SIZE;

		if (!pgentry_present(table, (*pde))) {
			if (type == MR_MODIFY) {
				pr_warn("%s, addr: 0x%lx pde is not present.\n", __func__, vaddr);
			}
		} else {
			if (pde_large(*pde) != 0UL) {
				if ((vaddr_next > vaddr_end) || (!mem_aligned_check(vaddr, PDE_SIZE))) {
					split_large_page(pde, IA32E_PD, vaddr, table);
				} else {
					local_modify_or_del_pte(pde, prot_set, prot_clr, type, table);
					if (vaddr_next < vaddr_end) {
						vaddr = vaddr_next;
						continue;
					}
					break;	/* done */
				}
			}
			modify_or_del_pte(pde, vaddr, vaddr_end, prot_set, prot_clr, table, type);
		}
		if (vaddr_next >= vaddr_end) {
			break;	/* done */
		}
		vaddr = vaddr_next;
	}

	try_to_free_pgtable_page(table, pdpte, pd_page, type);
}

/*
 * In PDPT level,
 * type: MR_MODIFY
 * modify [vaddr_start, vaddr_end) memory type or page access right.
 * type: MR_DEL
 * delete [vaddr_start, vaddr_end) MT PT mapping
 */
static void modify_or_del_pdpte(const uint64_t *pml4e, uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot_set, uint64_t prot_clr, const struct pgtable *table, uint32_t type)
{
	uint64_t *pdpt_page = pml4e_page_vaddr(*pml4e);
	uint64_t vaddr = vaddr_start;
	uint64_t index = pdpte_index(vaddr);

	dev_dbg(DBG_LEVEL_MMU, "%s, vaddr: [0x%lx - 0x%lx]\n", __func__, vaddr, vaddr_end);
	for (; index < PTRS_PER_PDPTE; index++) {
		uint64_t *pdpte = pdpt_page + index;
		uint64_t vaddr_next = (vaddr & PDPTE_MASK) + PDPTE_SIZE;

		if (!pgentry_present(table, (*pdpte))) {
			if (type == MR_MODIFY) {
				pr_warn("%s, vaddr: 0x%lx pdpte is not present.\n", __func__, vaddr);
			}
		} else {
			if (pdpte_large(*pdpte) != 0UL) {
				if ((vaddr_next > vaddr_end) ||
						(!mem_aligned_check(vaddr, PDPTE_SIZE))) {
					split_large_page(pdpte, IA32E_PDPT, vaddr, table);
				} else {
					local_modify_or_del_pte(pdpte, prot_set, prot_clr, type, table);
					if (vaddr_next < vaddr_end) {
						vaddr = vaddr_next;
						continue;
					}
					break;	/* done */
				}
			}
			modify_or_del_pde(pdpte, vaddr, vaddr_end, prot_set, prot_clr, table, type);
		}
		if (vaddr_next >= vaddr_end) {
			break;	/* done */
		}
		vaddr = vaddr_next;
	}
}

/**
 * @brief Modify or delete the mappings associated with the specified address range.
 *
 * This function modifies the properties of an existing mapping or deletes it entirely from the page table. The input
 * address range is specified by [vaddr_base, vaddr_base + size). It is used when changing the access permissions of a
 * memory region or when freeing a previously mapped region. This operation is critical for dynamic memory management,
 * allowing the system to adapt to changes in memory usage patterns or to reclaim resources.
 *
 * For error case behaviors:
 * - If the 'type' is MR_MODIFY and any page referenced by the PML4E in the specified address range is not present, the
 * function asserts that the operation is invalid.
 * For normal case behaviors(when the error case conditions are not satisfied):
 * - If any page referenced by the PDPTE/PDE/PTE in the specified address range is not present, there is no change to
 * the corresponding mapping and it continues the operation.
 * - If any PDPTE/PDE in the specified address range maps a large page and the large page address exceeds the specified
 * address range, the function splits the large page into next level page to allow for the modification or deletion of
 * the mappings and the execute right will be recovered by the callback function table->recover_exe_right() when a 2MB
 * page is split to 4KB pages.
 * - If the 'type' is MR_MODIFY, the function modifies the properties of the existing mapping to match the specified
 * properties.
 * - If the 'type' is MR_DEL, the function will set corresponding page table entries to point to the sanitized page.
 *
 * @param[inout] pml4_page A pointer to the specified PML4 table.
 * @param[in] vaddr_base The specified input address determining the start of the input address range whose mapping
 *                       information is to be updated.
 *                       For hypervisor's MMU, it is the host virtual address.
 *                       For each VM's EPT, it is the guest physical address.
 * @param[in] size The size of the specified input address range whose mapping information is to be updated.
 * @param[in] prot_set Bit positions representing the specified properties which need to be set.
 *                     Bits specified by prot_clr are cleared before each bit specified by prot_set is set to 1.
 * @param[in] prot_clr Bit positions representing the specified properties which need to be cleared.
 *                     Bits specified by prot_clr are cleared before each bit specified by prot_set is set to 1.
 * @param[in] table A pointer to the struct pgtable containing the information of the specified memory operations.
 * @param[in] type The type of operation to perform (MR_MODIFY or MR_DEL).
 *
 * @return None
 *
 * @pre pml4_page != NULL
 * @pre table != NULL
 * @pre (type == MR_MODIFY) || (type == MR_DEL)
 * @pre For x86 hypervisor, the following conditions shall be met if "type == MR_MODIFY".
 *      - (prot_set & ~(PAGE_RW | PAGE_USER | PAGE_PWT | PAGE_PCD | PAGE_ACCESSED | PAGE_DIRTY | PAGE_PSE | PAGE_GLOBAL
 *      | PAGE_PAT_LARGE | PAGE_NX) == 0)
 *      - (prot_clr & ~(PAGE_RW | PAGE_USER | PAGE_PWT | PAGE_PCD | PAGE_ACCESSED | PAGE_DIRTY | PAGE_PSE | PAGE_GLOBAL
 *      | PAGE_PAT_LARGE | PAGE_NX) == 0)
 * @pre For the VM EPT mappings, the following conditions shall be met if "type == MR_MODIFY".
 *      - (prot_set & ~(EPT_RD | EPT_WR | EPT_EXE | EPT_MT_MASK) == 0)
 *      - (prot_set & EPT_MT_MASK) == EPT_UNCACHED || (prot_set & EPT_MT_MASK) == EPT_WC ||
 *        (prot_set & EPT_MT_MASK) == EPT_WT || (prot_set & EPT_MT_MASK) == EPT_WP || (prot_set & EPT_MT_MASK) == EPT_WB
 *      - (prot_clr & ~(EPT_RD | EPT_WR | EPT_EXE | EPT_MT_MASK) == 0)
 *      - (prot_clr & EPT_MT_MASK) == EPT_UNCACHED || (prot_clr & EPT_MT_MASK) == EPT_WC ||
 *        (prot_clr & EPT_MT_MASK) == EPT_WT || (prot_clr & EPT_MT_MASK) == EPT_WP || (prot_clr & EPT_MT_MASK) == EPT_WB
 *
 * @post N/A
 *
 * @remark N/A
 */
void pgtable_modify_or_del_map(uint64_t *pml4_page, uint64_t vaddr_base, uint64_t size,
		uint64_t prot_set, uint64_t prot_clr, const struct pgtable *table, uint32_t type)
{
	uint64_t vaddr = round_page_up(vaddr_base);
	uint64_t vaddr_next, vaddr_end;
	uint64_t *pml4e;

	vaddr_end = vaddr + round_page_down(size);
	dev_dbg(DBG_LEVEL_MMU, "%s, vaddr: 0x%lx, size: 0x%lx\n",
		__func__, vaddr, size);

	while (vaddr < vaddr_end) {
		vaddr_next = (vaddr & PML4E_MASK) + PML4E_SIZE;
		pml4e = pml4e_offset(pml4_page, vaddr);
		if ((!pgentry_present(table, (*pml4e))) && (type == MR_MODIFY)) {
			ASSERT(false, "invalid op, pml4e not present");
		} else {
			modify_or_del_pdpte(pml4e, vaddr, vaddr_end, prot_set, prot_clr, table, type);
			vaddr = vaddr_next;
		}
	}
}

/*
 * In PT level,
 * add [vaddr_start, vaddr_end) to [paddr_base, ...) MT PT mapping
 */
static void add_pte(const uint64_t *pde, uint64_t paddr_start, uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot, const struct pgtable *table)
{
	uint64_t *pt_page = pde_page_vaddr(*pde);
	uint64_t vaddr = vaddr_start;
	uint64_t paddr = paddr_start;
	uint64_t index = pte_index(vaddr);

	dev_dbg(DBG_LEVEL_MMU, "%s, paddr: 0x%lx, vaddr: [0x%lx - 0x%lx]\n",
		__func__, paddr, vaddr_start, vaddr_end);
	for (; index < PTRS_PER_PTE; index++) {
		uint64_t *pte = pt_page + index;

		if (pgentry_present(table, (*pte))) {
			pr_fatal("%s, pte 0x%lx is already present!\n", __func__, vaddr);
		} else {
			set_pgentry(pte, paddr | prot, table);
		}
		paddr += PTE_SIZE;
		vaddr += PTE_SIZE;

		if (vaddr >= vaddr_end) {
			break;	/* done */
		}
	}
}

/*
 * In PD level,
 * add [vaddr_start, vaddr_end) to [paddr_base, ...) MT PT mapping
 */
static void add_pde(const uint64_t *pdpte, uint64_t paddr_start, uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot, const struct pgtable *table)
{
	uint64_t *pd_page = pdpte_page_vaddr(*pdpte);
	uint64_t vaddr = vaddr_start;
	uint64_t paddr = paddr_start;
	uint64_t index = pde_index(vaddr);
	uint64_t local_prot = prot;

	dev_dbg(DBG_LEVEL_MMU, "%s, paddr: 0x%lx, vaddr: [0x%lx - 0x%lx]\n",
		__func__, paddr, vaddr, vaddr_end);
	for (; index < PTRS_PER_PDE; index++) {
		uint64_t *pde = pd_page + index;
		uint64_t vaddr_next = (vaddr & PDE_MASK) + PDE_SIZE;

		if (pde_large(*pde) != 0UL) {
			pr_fatal("%s, pde 0x%lx is already present!\n", __func__, vaddr);
		} else {
			if (!pgentry_present(table, (*pde))) {
				if (table->large_page_support(IA32E_PD, prot) &&
					mem_aligned_check(paddr, PDE_SIZE) &&
					mem_aligned_check(vaddr, PDE_SIZE) &&
					(vaddr_next <= vaddr_end)) {
					table->tweak_exe_right(&local_prot);
					set_pgentry(pde, paddr | (local_prot | PAGE_PSE), table);
					if (vaddr_next < vaddr_end) {
						paddr += (vaddr_next - vaddr);
						vaddr = vaddr_next;
						continue;
					}
					break;	/* done */
				} else {
					void *pt_page = alloc_page(table->pool);
					construct_pgentry(pde, pt_page, table->default_access_right, table);
				}
			}
			add_pte(pde, paddr, vaddr, vaddr_end, prot, table);
		}
		if (vaddr_next >= vaddr_end) {
			break;	/* done */
		}
		paddr += (vaddr_next - vaddr);
		vaddr = vaddr_next;
	}
}

/*
 * In PDPT level,
 * add [vaddr_start, vaddr_end) to [paddr_base, ...) MT PT mapping
 */
static void add_pdpte(const uint64_t *pml4e, uint64_t paddr_start, uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot, const struct pgtable *table)
{
	uint64_t *pdpt_page = pml4e_page_vaddr(*pml4e);
	uint64_t vaddr = vaddr_start;
	uint64_t paddr = paddr_start;
	uint64_t index = pdpte_index(vaddr);
	uint64_t local_prot = prot;

	dev_dbg(DBG_LEVEL_MMU, "%s, paddr: 0x%lx, vaddr: [0x%lx - 0x%lx]\n", __func__, paddr, vaddr, vaddr_end);
	for (; index < PTRS_PER_PDPTE; index++) {
		uint64_t *pdpte = pdpt_page + index;
		uint64_t vaddr_next = (vaddr & PDPTE_MASK) + PDPTE_SIZE;

		if (pdpte_large(*pdpte) != 0UL) {
			pr_fatal("%s, pdpte 0x%lx is already present!\n", __func__, vaddr);
		} else {
			if (!pgentry_present(table, (*pdpte))) {
				if (table->large_page_support(IA32E_PDPT, prot) &&
					mem_aligned_check(paddr, PDPTE_SIZE) &&
					mem_aligned_check(vaddr, PDPTE_SIZE) &&
					(vaddr_next <= vaddr_end)) {
					table->tweak_exe_right(&local_prot);
					set_pgentry(pdpte, paddr | (local_prot | PAGE_PSE), table);
					if (vaddr_next < vaddr_end) {
						paddr += (vaddr_next - vaddr);
						vaddr = vaddr_next;
						continue;
					}
					break;	/* done */
				} else {
					void *pd_page = alloc_page(table->pool);
					construct_pgentry(pdpte, pd_page, table->default_access_right, table);
				}
			}
			add_pde(pdpte, paddr, vaddr, vaddr_end, prot, table);
		}
		if (vaddr_next >= vaddr_end) {
			break;	/* done */
		}
		paddr += (vaddr_next - vaddr);
		vaddr = vaddr_next;
	}
}

/**
 * @brief Add new page table mappings.
 *
 * This function maps a virtual address range specified by [vaddr_base, vaddr_base + size) to a physical address range
 * starting from 'paddr_base'.
 *
 * - If any subrange within [vaddr_base, vaddr_base + size) is already mapped, there is no change to the corresponding
 * mapping and it continues the operation.
 * - When a new 1GB or 2MB mapping is established, the callback function table->tweak_exe_right() is invoked to tweak
 * the execution bit.
 * - When a new page table referenced by a new PDPTE/PDE is created, all entries in the page table are initialized to
 * point to the sanitized page by default.
 * - Finally, the new mappings are established and initialized according to the specified address range and properties.
 *
 * @param[inout] pml4_page A pointer to the specified PML4 table hierarchy.
 * @param[in] paddr_base The specified physical address determining the start of the physical memory region.
 *                       It is the host physical address.
 * @param[in] vaddr_base The specified input address determining the start of the input address space.
 *                       For hypervisor's MMU, it is the host virtual address.
 *                       For each VM's EPT, it is the guest physical address.
 * @param[in] size The size of the specified input address space.
 * @param[in] prot Bit positions representing the specified properties which need to be set.
 * @param[in] table A pointer to the struct pgtable containing the information of the specified memory operations.
 *
 * @return None
 *
 * @pre pml4_page != NULL
 * @pre Any subrange within [vaddr_base, vaddr_base + size) shall already be unmapped.
 * @pre For x86 hypervisor mapping, the following condition shall be met.
 *      - prot & ~(PAGE_PRESENT| PAGE_RW | PAGE_USER | PAGE_PWT | PAGE_PCD | PAGE_ACCESSED | PAGE_DIRTY | PAGE_PSE |
 *      PAGE_GLOBAL | PAGE_PAT_LARGE | PAGE_NX) == 0
 * @pre For VM EPT mapping, the following conditions shall be met.
 *      - prot & ~(EPT_RD | EPT_WR | EPT_EXE | EPT_MT_MASK | EPT_IGNORE_PAT) == 0
 *      - (prot & EPT_MT_MASK) == EPT_UNCACHED || (prot & EPT_MT_MASK) == EPT_WC || (prot & EPT_MT_MASK) == EPT_WT ||
 *        (prot & EPT_MT_MASK) == EPT_WP || (prot & EPT_MT_MASK) == EPT_WB
 * @pre table != NULL
 *
 * @post N/A
 *
 * @remark N/A
 */
void pgtable_add_map(uint64_t *pml4_page, uint64_t paddr_base, uint64_t vaddr_base,
		uint64_t size, uint64_t prot, const struct pgtable *table)
{
	uint64_t vaddr, vaddr_next, vaddr_end;
	uint64_t paddr;
	uint64_t *pml4e;

	dev_dbg(DBG_LEVEL_MMU, "%s, paddr 0x%lx, vaddr 0x%lx, size 0x%lx\n", __func__, paddr_base, vaddr_base, size);

	/* align address to page size*/
	vaddr = round_page_up(vaddr_base);
	paddr = round_page_up(paddr_base);
	vaddr_end = vaddr + round_page_down(size);

	while (vaddr < vaddr_end) {
		vaddr_next = (vaddr & PML4E_MASK) + PML4E_SIZE;
		pml4e = pml4e_offset(pml4_page, vaddr);
		if (!pgentry_present(table, (*pml4e))) {
			void *pdpt_page = alloc_page(table->pool);
			construct_pgentry(pml4e, pdpt_page, table->default_access_right, table);
		}
		add_pdpte(pml4e, paddr, vaddr, vaddr_end, prot, table);

		paddr += (vaddr_next - vaddr);
		vaddr = vaddr_next;
	}
}

/**
 * @brief Create a new root page table.
 *
 * This function initializes and returns a new root page table. It is typically used during the setup of a new execution
 * context, such as initializing a hypervisor PML4 table or creating a virtual machine. The root page table is essential
 * for defining the virtual memory layout for the context.
 *
 * It creates a new root page table and every entries in the page table are initialized to point to the sanitized page.
 * Finally, the function returns the root page table pointer.
 *
 * @param[in] table A pointer to the struct pgtable containing the information of the specified memory operations.
 *
 * @return A pointer to the newly created root page table.
 *
 * @pre table != NULL
 *
 * @post N/A
 */
void *pgtable_create_root(const struct pgtable *table)
{
	uint64_t *page = (uint64_t *)alloc_page(table->pool);
	sanitize_pte(page, table);
	return page;
}

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
	set_pgentry((uint64_t *)pml4_base, sworld_pml4e, table);

	nworld_pml4e = get_pgentry((uint64_t *)nworld_pml4_page);

	/*
	 * copy PTPDEs from normal world EPT to secure world EPT,
	 * and remove execute access attribute in these entries
	 */
	dest_pdpte_p = pml4e_page_vaddr(sworld_pml4e);
	src_pdpte_p = pml4e_page_vaddr(nworld_pml4e);
	for (i = 0U; i < (uint16_t)(PTRS_PER_PDPTE - 1UL); i++) {
		pdpte = get_pgentry(src_pdpte_p);
		if ((pdpte & prot_table_present) != 0UL) {
			pdpte &= ~prot_clr;
			set_pgentry(dest_pdpte_p, pdpte, table);
		}
		src_pdpte_p++;
		dest_pdpte_p++;
	}

	return pml4_base;
}

/**
 * @brief Look for the paging-structure entry that contains the mapping information for the specified input address.
 *
 * This function looks for the paging-structure entry that contains the mapping information for the specified input
 * address of the translation process. It is used to search the page table hierarchy for the entry corresponding to the
 * given virtual address. The function traverses the page table hierarchy from the PML4 down to the appropriate page
 * table level, returning the entry if found.
 *
 * - If specified address is mapped in the page table hierarchy, it will return a pointer to the page table entry that
 * maps the specified address.
 * - If the specified address is not mapped in the page table hierarchy, it will return NULL.
 *
 * @param[in] pml4_page A pointer to the specified PML4 table hierarchy.
 * @param[in] addr The specified input address whose mapping information is to be searched.
 *                 For hypervisor's MMU, it is the host virtual address.
 *                 For each VM's EPT, it is the guest physical address.
 * @param[out] pg_size A pointer to the size of the page controlled by the returned paging-structure entry.
 * @param[in] table A pointer to the struct pgtable which provides the page pool and callback functions to be used when
 *                  creating the new page.
 *
 * @return A pointer to the paging-structure entry that maps the specified input address.
 *
 * @retval non-NULL There is a paging-structure entry that contains the mapping information for the specified input
 *                  address.
 * @retval NULL There is no paging-structure entry that contains the mapping information for the specified input
 *              address.
 *
 * @pre pml4_page != NULL
 * @pre pg_size != NULL
 * @pre table != NULL
 *
 * @post N/A
 *
 * @remark N/A
 */
const uint64_t *pgtable_lookup_entry(uint64_t *pml4_page, uint64_t addr, uint64_t *pg_size, const struct pgtable *table)
{
	const uint64_t *pret = NULL;
	bool present = true;
	uint64_t *pml4e, *pdpte, *pde, *pte;

	pml4e = pml4e_offset(pml4_page, addr);
	present = pgentry_present(table, (*pml4e));

	if (present) {
		pdpte = pdpte_offset(pml4e, addr);
		present = pgentry_present(table, (*pdpte));
		if (present) {
			if (pdpte_large(*pdpte) != 0UL) {
				*pg_size = PDPTE_SIZE;
				pret = pdpte;
			} else {
				pde = pde_offset(pdpte, addr);
				present = pgentry_present(table, (*pde));
				if (present) {
					if (pde_large(*pde) != 0UL) {
						*pg_size = PDE_SIZE;
						pret = pde;
					} else {
						pte = pte_offset(pde, addr);
						present = pgentry_present(table, (*pte));
						if (present) {
							*pg_size = PTE_SIZE;
							pret = pte;
						}
					}
				}
			}
		}
	}

	return pret;
}

/**
 * @}
 */