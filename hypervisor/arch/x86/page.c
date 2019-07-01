/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <rtl.h>
#include <pgtable.h>
#include <page.h>
#include <mmu.h>
#include <vm.h>
#include <trusty.h>
#include <vtd.h>

static struct page ppt_pml4_pages[PML4_PAGE_NUM(CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE)];
static struct page ppt_pdpt_pages[PDPT_PAGE_NUM(CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE)];
static struct page ppt_pd_pages[PD_PAGE_NUM(CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE)];

/* ppt: pripary page table */
static union pgtable_pages_info ppt_pages_info = {
	.ppt = {
		.pml4_base = ppt_pml4_pages,
		.pdpt_base = ppt_pdpt_pages,
		.pd_base = ppt_pd_pages,
	}
};

static inline uint64_t ppt_get_default_access_right(void)
{
	return (PAGE_PRESENT | PAGE_RW | PAGE_USER);
}

static inline void ppt_clflush_pagewalk(const void* etry __attribute__((unused)))
{
}

static inline uint64_t ppt_pgentry_present(uint64_t pte)
{
	return pte & PAGE_PRESENT;
}

static inline struct page *ppt_get_pml4_page(const union pgtable_pages_info *info)
{
	struct page *pml4_page = info->ppt.pml4_base;
	(void)memset(pml4_page, 0U, PAGE_SIZE);
	return pml4_page;
}

static inline struct page *ppt_get_pdpt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *pdpt_page = info->ppt.pdpt_base + (gpa >> PML4E_SHIFT);
	(void)memset(pdpt_page, 0U, PAGE_SIZE);
	return pdpt_page;
}

static inline struct page *ppt_get_pd_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *pd_page = info->ppt.pd_base + (gpa >> PDPTE_SHIFT);
	(void)memset(pd_page, 0U, PAGE_SIZE);
	return pd_page;
}

const struct memory_ops ppt_mem_ops = {
	.info = &ppt_pages_info,
	.get_default_access_right = ppt_get_default_access_right,
	.pgentry_present = ppt_pgentry_present,
	.get_pml4_page = ppt_get_pml4_page,
	.get_pdpt_page = ppt_get_pdpt_page,
	.get_pd_page = ppt_get_pd_page,
	.clflush_pagewalk = ppt_clflush_pagewalk,
};

static struct page sos_vm_pml4_pages[PML4_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE))];
static struct page sos_vm_pdpt_pages[PDPT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE))];
static struct page sos_vm_pd_pages[PD_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE))];
static struct page sos_vm_pt_pages[PT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE))];

/* uos_nworld_pml4_pages[i] is ...... of UOS i (whose vm_id = i +1) */
static struct page uos_nworld_pml4_pages[CONFIG_MAX_VM_NUM - 1U][PML4_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
static struct page uos_nworld_pdpt_pages[CONFIG_MAX_VM_NUM - 1U][PDPT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
static struct page uos_nworld_pd_pages[CONFIG_MAX_VM_NUM - 1U][PD_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
static struct page uos_nworld_pt_pages[CONFIG_MAX_VM_NUM - 1U][PT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];

static struct page uos_sworld_pgtable_pages[CONFIG_MAX_VM_NUM - 1U][TRUSTY_PGTABLE_PAGE_NUM(TRUSTY_RAM_SIZE)];
/* pre-assumption: TRUSTY_RAM_SIZE is 2M aligned */
static struct page uos_sworld_memory[CONFIG_MAX_VM_NUM - 1U][TRUSTY_RAM_SIZE >> PAGE_SHIFT] __aligned(MEM_2M);

/* ept: extended page table*/
static union pgtable_pages_info ept_pages_info[CONFIG_MAX_VM_NUM] = {
	{
		.ept = {
			.top_address_space = EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE),
			.nworld_pml4_base = sos_vm_pml4_pages,
			.nworld_pdpt_base = sos_vm_pdpt_pages,
			.nworld_pd_base = sos_vm_pd_pages,
			.nworld_pt_base = sos_vm_pt_pages,
		},
	},
};

void *get_reserve_sworld_memory_base(void)
{
	return uos_sworld_memory;
}

static inline uint64_t ept_get_default_access_right(void)
{
	return EPT_RWX;
}

static inline uint64_t ept_pgentry_present(uint64_t pte)
{
	return pte & EPT_RWX;
}

static inline void ept_clflush_pagewalk(const void* etry)
{
	iommu_flush_cache(etry, sizeof(uint64_t));
}

static inline struct page *ept_get_pml4_page(const union pgtable_pages_info *info)
{
	struct page *pml4_page = info->ept.nworld_pml4_base;
	(void)memset(pml4_page, 0U, PAGE_SIZE);
	return pml4_page;
}

static inline struct page *ept_get_pdpt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *pdpt_page = info->ept.nworld_pdpt_base + (gpa >> PML4E_SHIFT);
	(void)memset(pdpt_page, 0U, PAGE_SIZE);
	return pdpt_page;
}

static inline struct page *ept_get_pd_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *pd_page;
	if (gpa < TRUSTY_EPT_REBASE_GPA) {
		pd_page = info->ept.nworld_pd_base + (gpa >> PDPTE_SHIFT);
	} else {
		pd_page = info->ept.sworld_pgtable_base + TRUSTY_PML4_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) +
			TRUSTY_PDPT_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) + ((gpa - TRUSTY_EPT_REBASE_GPA) >> PDPTE_SHIFT);
	}
	(void)memset(pd_page, 0U, PAGE_SIZE);
	return pd_page;
}

static inline struct page *ept_get_pt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *pt_page;
	if (gpa < TRUSTY_EPT_REBASE_GPA) {
		pt_page = info->ept.nworld_pt_base + (gpa >> PDE_SHIFT);
	} else {
		pt_page = info->ept.sworld_pgtable_base + TRUSTY_PML4_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) +
			TRUSTY_PDPT_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) + TRUSTY_PD_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) +
			((gpa - TRUSTY_EPT_REBASE_GPA) >> PDE_SHIFT);
	}
	(void)memset(pt_page, 0U, PAGE_SIZE);
	return pt_page;
}

static inline void *ept_get_sworld_memory_base(const union pgtable_pages_info *info)
{
	return info->ept.sworld_memory_base;
}

void init_ept_mem_ops(struct acrn_vm *vm)
{
	uint16_t vm_id = vm->vm_id;
	if (vm_id != 0U) {
		ept_pages_info[vm_id].ept.top_address_space = EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE);
		ept_pages_info[vm_id].ept.nworld_pml4_base = uos_nworld_pml4_pages[vm_id - 1U];
		ept_pages_info[vm_id].ept.nworld_pdpt_base = uos_nworld_pdpt_pages[vm_id - 1U];
		ept_pages_info[vm_id].ept.nworld_pd_base = uos_nworld_pd_pages[vm_id - 1U];
		ept_pages_info[vm_id].ept.nworld_pt_base = uos_nworld_pt_pages[vm_id - 1U];
		ept_pages_info[vm_id].ept.sworld_pgtable_base = uos_sworld_pgtable_pages[vm_id - 1U];
		ept_pages_info[vm_id].ept.sworld_memory_base = uos_sworld_memory[vm_id - 1U];

		vm->arch_vm.ept_mem_ops.get_sworld_memory_base = ept_get_sworld_memory_base;
	}
	vm->arch_vm.ept_mem_ops.info = &ept_pages_info[vm_id];

	vm->arch_vm.ept_mem_ops.get_default_access_right = ept_get_default_access_right;
	vm->arch_vm.ept_mem_ops.pgentry_present = ept_pgentry_present;
	vm->arch_vm.ept_mem_ops.get_pml4_page = ept_get_pml4_page;
	vm->arch_vm.ept_mem_ops.get_pdpt_page = ept_get_pdpt_page;
	vm->arch_vm.ept_mem_ops.get_pd_page = ept_get_pd_page;
	vm->arch_vm.ept_mem_ops.get_pt_page = ept_get_pt_page;
	vm->arch_vm.ept_mem_ops.clflush_pagewalk = ept_clflush_pagewalk;
}
