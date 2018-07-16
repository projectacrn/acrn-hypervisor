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
		set_pte(pbase + i, paddr | ref_prot);
		paddr += paddrinc;
	}

	ref_prot = (ptt == PTT_HOST) ? PAGE_TABLE : EPT_RWX;
	set_pte(pte, HVA2HPA((void *)pbase) | ref_prot);

	/* TODO: flush the TLB */

	return 0;
}

/*
 * In PT level, modify [vaddr_start, vaddr_end) MR PTA.
 */
static int modify_pte(uint64_t *pde,
		uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot_set, uint64_t prot_clr,
		enum _page_table_type ptt)
{
	uint64_t *pd_page = pde_page_vaddr(*pde);
	uint64_t vaddr = vaddr_start;
	uint64_t index = pte_index(vaddr);

	dev_dbg(ACRN_DBG_MMU, "%s, vaddr: [0x%llx - 0x%llx]\n",
		__func__, vaddr, vaddr_end);
	for (; index < PTRS_PER_PTE; index++) {
		uint64_t new_pte, *pte = pd_page + index;
		uint64_t vaddr_next = (vaddr & PTE_MASK) + PTE_SIZE;

		if (pgentry_present(ptt, *pte) == 0UL) {
			pr_err("%s, invalid op, pte not present\n", __func__);
			return -EFAULT;
		}

		new_pte = *pte;
		new_pte &= ~prot_clr;
		new_pte |= prot_set;
		set_pte(pte, new_pte);

		if (vaddr_next >= vaddr_end) {
			break;
		}
		vaddr = vaddr_next;
	}

	return 0;
}

/*
 * In PD level, modify [vaddr_start, vaddr_end) MR PTA.
 */
static int modify_pde(uint64_t *pdpte,
		uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot_set, uint64_t prot_clr,
		enum _page_table_type ptt)
{
	int ret = 0;
	uint64_t *pdpt_page = pdpte_page_vaddr(*pdpte);
	uint64_t vaddr = vaddr_start;
	uint64_t index = pde_index(vaddr);

	dev_dbg(ACRN_DBG_MMU, "%s, vaddr: [0x%llx - 0x%llx]\n",
		__func__, vaddr, vaddr_end);
	for (; index < PTRS_PER_PDE; index++) {
		uint64_t *pde = pdpt_page + index;
		uint64_t vaddr_next = (vaddr & PDE_MASK) + PDE_SIZE;

		if (pgentry_present(ptt, *pde) == 0UL) {
			pr_err("%s, invalid op, pde not present\n", __func__);
			return -EFAULT;
		}
		if (pde_large(*pde) != 0UL) {
			if (vaddr_next > vaddr_end) {
				ret = split_large_page(pde, IA32E_PD, ptt);
				if (ret != 0) {
					return ret;
				}
			} else {
				uint64_t new_pde = *pde;
				new_pde &= ~prot_clr;
				new_pde |= prot_set;
				set_pte(pde, new_pde);
				if (vaddr_next < vaddr_end) {
					vaddr = vaddr_next;
					continue;
				}
				return 0;
			}
		}
		ret = modify_pte(pde, vaddr, vaddr_end,
				prot_set, prot_clr, ptt);
		if (ret != 0 || (vaddr_next >= vaddr_end)) {
			return ret;
		}
		vaddr = vaddr_next;
	}

	return ret;
}

/*
 * In PDPT level, modify [vaddr, vaddr_end) MR PTA.
 */
static int modify_pdpte(uint64_t *pml4e,
		uint64_t vaddr_start, uint64_t vaddr_end,
		uint64_t prot_set, uint64_t prot_clr,
		enum _page_table_type ptt)
{
	int ret = 0;
	uint64_t *pml4_page = pml4e_page_vaddr(*pml4e);
	uint64_t vaddr = vaddr_start;
	uint64_t index = pdpte_index(vaddr);

	dev_dbg(ACRN_DBG_MMU, "%s, vaddr: [0x%llx - 0x%llx]\n",
		__func__, vaddr, vaddr_end);
	for (; index < PTRS_PER_PDPTE; index++) {
		uint64_t *pdpte = pml4_page + index;
		uint64_t vaddr_next = (vaddr & PDPTE_MASK) + PDPTE_SIZE;

		if (pgentry_present(ptt, *pdpte) == 0UL) {
			pr_err("%s, invalid op, pdpte not present\n", __func__);
			return -EFAULT;
		}
		if (pdpte_large(*pdpte) != 0UL) {
			if (vaddr_next > vaddr_end) {
				ret = split_large_page(pdpte, IA32E_PDPT, ptt);
				if (ret != 0) {
					return ret;
				}
			} else {
				uint64_t new_pdpte = *pdpte;
				new_pdpte &= ~prot_clr;
				new_pdpte |= prot_set;
				set_pte(pdpte, new_pdpte);
				if (vaddr_next < vaddr_end) {
					vaddr = vaddr_next;
					continue;
				}
				return 0;
			}
		}
		ret = modify_pde(pdpte, vaddr, vaddr_end,
				prot_set, prot_clr, ptt);
		if (ret != 0 || (vaddr_next >= vaddr_end)) {
			return ret;
		}
		vaddr = vaddr_next;
	}

	return ret;
}

/*
 * modify [vaddr, vaddr + size ) memory region page table attributes.
 * prot_clr - attributes want to be clear
 * prot_set - attributes want to be set
 * @pre: the prot_set and prot_clr should set before call this function.
 * If you just want to modify access rights, you can just set the prot_clr
 * to what you want to set, prot_clr to what you want to clear. But if you
 * want to modify the MT, you should set the prot_set to what MT you want
 * to set, prot_clr to the MT mask.
 */
int mmu_modify(uint64_t *pml4_page,
		uint64_t vaddr_base, uint64_t size,
		uint64_t prot_set, uint64_t prot_clr,
		enum _page_table_type ptt)
{
	uint64_t vaddr = vaddr_base;
	uint64_t vaddr_next, vaddr_end;
	uint64_t *pml4e;
	int ret;

	if (!MEM_ALIGNED_CHECK(vaddr, PAGE_SIZE_4K) ||
		!MEM_ALIGNED_CHECK(size, PAGE_SIZE_4K)) {
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
		ret = modify_pdpte(pml4e, vaddr, vaddr_end,
					prot_set, prot_clr, ptt);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}
