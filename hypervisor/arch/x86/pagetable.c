/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <hypervisor.h>

#define ACRN_DBG_MMU	6U

/*
 * Split a large page table into next level page table.
 */
static int split_large_page(uint64_t *pte,
			enum _page_table_level level,
			enum _page_table_type ptt)
{
	int ret = -EINVAL;
	uint64_t *pbase;
	uint64_t ref_paddr, paddr, paddrinc;
	uint64_t i, ref_prot;

	switch (level) {
	case IA32E_PDPT:
		ref_paddr = (*pte) & PDPTE_PFN_MASK;
		paddrinc = PDE_SIZE;
		ref_prot = (*pte) & ~PDPTE_PFN_MASK;
		break;
	case IA32E_PD:
		ref_paddr = (*pte) & PDE_PFN_MASK;
		paddrinc = PTE_SIZE;
		ref_prot = (*pte) & ~PDE_PFN_MASK;
		ref_prot &= ~PAGE_PSE;
		break;
	default:
		return ret;
	}

	dev_dbg(ACRN_DBG_MMU, "%s, paddr: 0x%llx\n", __func__, ref_paddr);

	pbase = (uint64_t *)alloc_paging_struct();
	if (pbase == NULL) {
		return -ENOMEM;
	}

	paddr = ref_paddr;
	for (i = 0UL; i < PTRS_PER_PTE; i++) {
		set_pgentry(pbase + i, paddr | ref_prot);
		paddr += paddrinc;
	}

	ref_prot = (ptt == PTT_HOST) ? PAGE_TABLE : EPT_RWX;
	set_pgentry(pte, HVA2HPA((void *)pbase) | ref_prot);

	/* TODO: flush the TLB */

	return 0;
}

static inline void __modify_or_del_pte(uint64_t *pte,
		uint64_t prot_set, uint64_t prot_clr, uint32_t type)
{
	if (type == MR_MODIFY) {
		uint64_t new_pte = *pte;
		new_pte &= ~prot_clr;
		new_pte |= prot_set;
		set_pgentry(pte, new_pte);
	} else {
		set_pgentry(pte, 0);
	}
}

/*
 * pgentry may means pml4e/pdpte/pde
 */
static inline int construct_pgentry(enum _page_table_type ptt, uint64_t *pde)
{
	uint64_t prot;
	void *pd_page = alloc_paging_struct();
	if (pd_page == NULL) {
		return -ENOMEM;
	}

	prot = (ptt == PTT_HOST) ? PAGE_TABLE: EPT_RWX;
	set_pgentry(pde, HVA2HPA(pd_page) | prot);
	return 0;
}

/*
 * In PT level,
 * type: MR_MODIFY
 * modify [vaddr_start, vaddr_end) memory type or page access right.
 * type: MR_DEL
 * delete [vaddr_start, vaddr_end) MT PT mapping
 */
static int modify_or_del_pte(uint64_t *pde,
		uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot_set, uint64_t prot_clr,
		enum _page_table_type ptt, uint32_t type)
{
	uint64_t *pt_page = pde_page_vaddr(*pde);
	uint64_t vaddr = vaddr_start;
	uint64_t index = pte_index(vaddr);

	dev_dbg(ACRN_DBG_MMU, "%s, vaddr: [0x%llx - 0x%llx]\n",
		__func__, vaddr, vaddr_end);
	for (; index < PTRS_PER_PTE; index++) {
		uint64_t *pte = pt_page + index;

		if (pgentry_present(ptt, *pte) == 0UL) {
			pr_err("%s, invalid op, pte not present\n", __func__);
			return -EFAULT;
		}

		__modify_or_del_pte(pte, prot_set, prot_clr, type);
		vaddr += PTE_SIZE;
		if (vaddr >= vaddr_end) {
			break;
		}
	}

	return 0;
}

/*
 * In PD level,
 * type: MR_MODIFY
 * modify [vaddr_start, vaddr_end) memory type or page access right.
 * type: MR_DEL
 * delete [vaddr_start, vaddr_end) MT PT mapping
 */
static int modify_or_del_pde(uint64_t *pdpte,
		uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot_set, uint64_t prot_clr,
		enum _page_table_type ptt, uint32_t type)
{
	int ret = 0;
	uint64_t *pd_page = pdpte_page_vaddr(*pdpte);
	uint64_t vaddr = vaddr_start;
	uint64_t index = pde_index(vaddr);

	dev_dbg(ACRN_DBG_MMU, "%s, vaddr: [0x%llx - 0x%llx]\n",
		__func__, vaddr, vaddr_end);
	for (; index < PTRS_PER_PDE; index++) {
		uint64_t *pde = pd_page + index;
		uint64_t vaddr_next = (vaddr & PDE_MASK) + PDE_SIZE;

		if (pgentry_present(ptt, *pde) == 0UL) {
			pr_err("%s, invalid op, pde not present\n", __func__);
			return -EFAULT;
		}
		if (pde_large(*pde) != 0UL) {
			if (vaddr_next > vaddr_end ||
					!MEM_ALIGNED_CHECK(vaddr, PDE_SIZE)) {
				ret = split_large_page(pde, IA32E_PD, ptt);
				if (ret != 0) {
					return ret;
				}
			} else {
				__modify_or_del_pte(pde,
					prot_set, prot_clr, type);
				if (vaddr_next < vaddr_end) {
					vaddr = vaddr_next;
					continue;
				}
				return 0;
			}
		}
		ret = modify_or_del_pte(pde, vaddr, vaddr_end,
				prot_set, prot_clr, ptt, type);
		if (ret != 0 || (vaddr_next >= vaddr_end)) {
			return ret;
		}
		vaddr = vaddr_next;
	}

	return ret;
}

/*
 * In PDPT level,
 * type: MR_MODIFY
 * modify [vaddr_start, vaddr_end) memory type or page access right.
 * type: MR_DEL
 * delete [vaddr_start, vaddr_end) MT PT mapping
 */
static int modify_or_del_pdpte(uint64_t *pml4e,
		uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot_set, uint64_t prot_clr,
		enum _page_table_type ptt, uint32_t type)
{
	int ret = 0;
	uint64_t *pdpt_page = pml4e_page_vaddr(*pml4e);
	uint64_t vaddr = vaddr_start;
	uint64_t index = pdpte_index(vaddr);

	dev_dbg(ACRN_DBG_MMU, "%s, vaddr: [0x%llx - 0x%llx]\n",
		__func__, vaddr, vaddr_end);
	for (; index < PTRS_PER_PDPTE; index++) {
		uint64_t *pdpte = pdpt_page + index;
		uint64_t vaddr_next = (vaddr & PDPTE_MASK) + PDPTE_SIZE;

		if (pgentry_present(ptt, *pdpte) == 0UL) {
			pr_err("%s, invalid op, pdpte not present\n", __func__);
			return -EFAULT;
		}
		if (pdpte_large(*pdpte) != 0UL) {
			if (vaddr_next > vaddr_end ||
					!MEM_ALIGNED_CHECK(vaddr, PDPTE_SIZE)) {
				ret = split_large_page(pdpte, IA32E_PDPT, ptt);
				if (ret != 0) {
					return ret;
				}
			} else {
				__modify_or_del_pte(pdpte,
					prot_set, prot_clr, type);
				if (vaddr_next < vaddr_end) {
					vaddr = vaddr_next;
					continue;
				}
				return 0;
			}
		}
		ret = modify_or_del_pde(pdpte, vaddr, vaddr_end,
				prot_set, prot_clr, ptt, type);
		if (ret != 0 || (vaddr_next >= vaddr_end)) {
			return ret;
		}
		vaddr = vaddr_next;
	}

	return ret;
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
int mmu_modify_or_del(uint64_t *pml4_page,
		uint64_t vaddr_base, uint64_t size,
		uint64_t prot_set, uint64_t prot_clr,
		enum _page_table_type ptt, uint32_t type)
{
	uint64_t vaddr = vaddr_base;
	uint64_t vaddr_next, vaddr_end;
	uint64_t *pml4e;
	int ret;

	if (!MEM_ALIGNED_CHECK(vaddr, PAGE_SIZE_4K) ||
		!MEM_ALIGNED_CHECK(size, PAGE_SIZE_4K) ||
		(type != MR_MODIFY && type != MR_DEL)) {
		pr_err("%s, invalid parameters!\n", __func__);
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_MMU, "%s, vaddr: 0x%llx, size: 0x%llx\n",
		__func__, vaddr, size);
	vaddr_end = vaddr + size;
	for (; vaddr < vaddr_end; vaddr = vaddr_next) {
		vaddr_next = (vaddr & PML4E_MASK) + PML4E_SIZE;
		pml4e = pml4e_offset(pml4_page, vaddr);
		if (pgentry_present(ptt, *pml4e) == 0UL) {
			pr_err("%s, invalid op, pml4e not present\n", __func__);
			return -EFAULT;
		}
		ret = modify_or_del_pdpte(pml4e, vaddr, vaddr_end,
					prot_set, prot_clr, ptt, type);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

/*
 * In PT level,
 * add [vaddr_start, vaddr_end) to [paddr_base, ...) MT PT mapping
 */
static int add_pte(uint64_t *pde, uint64_t paddr_start,
		uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot, enum _page_table_type ptt)
{
	uint64_t *pt_page = pde_page_vaddr(*pde);
	uint64_t vaddr = vaddr_start;
	uint64_t paddr = paddr_start;
	uint64_t index = pte_index(vaddr);

	dev_dbg(ACRN_DBG_MMU, "%s, paddr: 0x%llx, vaddr: [0x%llx - 0x%llx]\n",
		__func__, paddr, vaddr_start, vaddr_end);
	for (; index < PTRS_PER_PTE; index++) {
		uint64_t *pte = pt_page + index;

		if (pgentry_present(ptt, *pte) != 0UL) {
			pr_err("%s, invalid op, pte present\n", __func__);
			return -EFAULT;
		}

		set_pgentry(pte, paddr | prot);
		paddr += PTE_SIZE;
		vaddr += PTE_SIZE;
		if (vaddr >= vaddr_end)
			return 0;
	}

	return 0;
}

/*
 * In PD level,
 * add [vaddr_start, vaddr_end) to [paddr_base, ...) MT PT mapping
 */
static int add_pde(uint64_t *pdpte, uint64_t paddr_start,
		uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot, enum _page_table_type ptt)
{
	int ret = 0;
	uint64_t *pd_page = pdpte_page_vaddr(*pdpte);
	uint64_t vaddr = vaddr_start;
	uint64_t paddr = paddr_start;
	uint64_t index = pde_index(vaddr);

	dev_dbg(ACRN_DBG_MMU, "%s, paddr: 0x%llx, vaddr: [0x%llx - 0x%llx]\n",
		__func__, paddr, vaddr, vaddr_end);
	for (; index < PTRS_PER_PDE; index++) {
		uint64_t *pde = pd_page + index;
		uint64_t vaddr_next = (vaddr & PDE_MASK) + PDE_SIZE;

		if (pgentry_present(ptt, *pde) == 0UL) {
			if (MEM_ALIGNED_CHECK(paddr, PDE_SIZE) &&
				MEM_ALIGNED_CHECK(vaddr, PDE_SIZE) &&
				(vaddr_next <= vaddr_end)) {
				set_pgentry(pde, paddr | (prot | PAGE_PSE));
				if (vaddr_next < vaddr_end) {
					paddr += (vaddr_next - vaddr);
					vaddr = vaddr_next;
					continue;
				}
				return 0;
			} else {
				ret = construct_pgentry(ptt, pde);
				if (ret != 0) {
					return ret;
				}
			}
		}
		ret = add_pte(pde, paddr, vaddr, vaddr_end, prot, ptt);
		if (ret != 0 || (vaddr_next >= vaddr_end)) {
			return ret;
		}
		paddr += (vaddr_next - vaddr);
		vaddr = vaddr_next;
	}

	return ret;
}

/*
 * In PDPT level,
 * add [vaddr_start, vaddr_end) to [paddr_base, ...) MT PT mapping
 */
static int add_pdpte(uint64_t *pml4e, uint64_t paddr_start,
		uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot, enum _page_table_type ptt)
{
	int ret = 0;
	uint64_t *pdpt_page = pml4e_page_vaddr(*pml4e);
	uint64_t vaddr = vaddr_start;
	uint64_t paddr = paddr_start;
	uint64_t index = pdpte_index(vaddr);

	dev_dbg(ACRN_DBG_MMU, "%s, paddr: 0x%llx, vaddr: [0x%llx - 0x%llx]\n",
		__func__, paddr, vaddr, vaddr_end);
	for (; index < PTRS_PER_PDPTE; index++) {
		uint64_t *pdpte = pdpt_page + index;
		uint64_t vaddr_next = (vaddr & PDPTE_MASK) + PDPTE_SIZE;

		if (pgentry_present(ptt, *pdpte) == 0UL) {
			if (MEM_ALIGNED_CHECK(paddr, PDPTE_SIZE) &&
				MEM_ALIGNED_CHECK(vaddr, PDPTE_SIZE) &&
				(vaddr_next <= vaddr_end)) {
				set_pgentry(pdpte, paddr | (prot | PAGE_PSE));
				if (vaddr_next < vaddr_end) {
					paddr += (vaddr_next - vaddr);
					vaddr = vaddr_next;
					continue;
				}
				return 0;
			} else {
				ret = construct_pgentry(ptt, pdpte);
				if (ret != 0) {
					return ret;
				}
			}
		}
		ret = add_pde(pdpte, paddr, vaddr, vaddr_end, prot, ptt);
		if (ret != 0 || (vaddr_next >= vaddr_end)) {
			return ret;
		}
		paddr += (vaddr_next - vaddr);
		vaddr = vaddr_next;
	}

	return ret;
}

/*
 * action: MR_ADD
 * add [vaddr_base, vaddr_base + size ) memory region page table mapping.
 * @pre: the prot should set before call this function.
 */
int mmu_add(uint64_t *pml4_page, uint64_t paddr_base,
		uint64_t vaddr_base, uint64_t size,
		uint64_t prot, enum _page_table_type ptt)
{
	uint64_t vaddr, vaddr_next, vaddr_end;
	uint64_t paddr;
	uint64_t *pml4e;
	int ret;

	dev_dbg(ACRN_DBG_MMU, "%s, paddr 0x%llx, vaddr 0x%llx, size 0x%llx\n",
		__func__, paddr_base, vaddr_base, size);

	/* align address to page size*/
	vaddr = ROUND_PAGE_UP(vaddr_base);
	paddr = ROUND_PAGE_UP(paddr_base);
	vaddr_end = vaddr + ROUND_PAGE_DOWN(size);

	for (; vaddr < vaddr_end; vaddr = vaddr_next) {
		vaddr_next = (vaddr & PML4E_MASK) + PML4E_SIZE;
		pml4e = pml4e_offset(pml4_page, vaddr);
		if (pgentry_present(ptt, *pml4e) == 0UL) {
			ret = construct_pgentry(ptt, pml4e);
			if (ret != 0) {
				return ret;
			}
		}
		ret = add_pdpte(pml4e, paddr, vaddr, vaddr_end, prot, ptt);
		if (ret != 0) {
			return ret;
		}

		paddr += (vaddr_next - vaddr);
	}

	return 0;
}

uint64_t *lookup_address(uint64_t *pml4_page,
		uint64_t addr, uint64_t *pg_size, enum _page_table_type ptt)
{
	uint64_t *pml4e, *pdpte, *pde, *pte;

	if ((pml4_page == NULL) || (pg_size == NULL)) {
		return NULL;
	}

	pml4e = pml4e_offset(pml4_page, addr);
	if (pgentry_present(ptt, *pml4e) == 0UL) {
		return NULL;
	}

	pdpte = pdpte_offset(pml4e, addr);
	if (pgentry_present(ptt, *pdpte) == 0UL) {
		return NULL;
	} else if (pdpte_large(*pdpte) != 0UL) {
		*pg_size = PDPTE_SIZE;
		return pdpte;
	}

	pde = pde_offset(pdpte, addr);
	if (pgentry_present(ptt, *pde) == 0UL) {
		return NULL;
	} else if (pde_large(*pde) != 0UL) {
		*pg_size = PDE_SIZE;
		return pde;
	}

	pte = pte_offset(pde, addr);
	if (pgentry_present(ptt, *pte) == 0UL) {
		return NULL;
	} else {
		*pg_size = PTE_SIZE;
		return pte;
	}
}
