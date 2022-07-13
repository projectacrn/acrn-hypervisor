/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <util.h>
#include <acrn_hv_defs.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <logmsg.h>

#define DBG_LEVEL_MMU	6U

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

/*
 * type: MR_MODIFY
 * modify [vaddr, vaddr + size ) memory type or page access right.
 * prot_clr - memory type or page access right want to be clear
 * prot_set - memory type or page access right want to be set
 * @pre: the prot_set and prot_clr should set before call this function.
 * If you just want to modify access rights, you can just set the prot_clr
 * to what you want to set, prot_clr to what you want to clear. But if you
 * want to modify the MT, you should set the prot_set to what MT you want
 * to set, prot_clr to the MT mask.
 * type: MR_DEL
 * delete [vaddr_base, vaddr_base + size ) memory region page table mapping.
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

/*
 * action: MR_ADD
 * add [vaddr_base, vaddr_base + size ) memory region page table mapping.
 * @pre: the prot should set before call this function.
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

void *pgtable_create_root(const struct pgtable *table)
{
	uint64_t *page = (uint64_t *)alloc_page(table->pool);
	sanitize_pte(page, table);
	return page;
}

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
	 * Normal World.PD/PT are shared in both Secure world's EPT
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
 * @pre (pml4_page != NULL) && (pg_size != NULL)
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
