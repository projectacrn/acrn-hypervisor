/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <hypervisor.h>

#define DEFINE_PGTABLE_PAGE(prefix, lvl, LVL, size)	\
		static struct page prefix ## lvl ## _pages[LVL ## _PAGE_NUM(size)]

DEFINE_PGTABLE_PAGE(ppt_, pml4, PML4, CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE);
DEFINE_PGTABLE_PAGE(ppt_, pdpt, PDPT, CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE);
DEFINE_PGTABLE_PAGE(ppt_, pd, PD, CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE);

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

static inline uint64_t ppt_pgentry_present(uint64_t pte)
{
	return pte & PAGE_PRESENT;
}

static inline struct page *ppt_get_pml4_page(const union pgtable_pages_info *info, __unused uint64_t gpa)
{
	struct page *page = info->ppt.pml4_base;
	(void)memset(page, 0U, PAGE_SIZE);
	return page;
}

static inline struct page *ppt_get_pdpt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *page = info->ppt.pdpt_base + (gpa >> PML4E_SHIFT);
	(void)memset(page, 0U, PAGE_SIZE);
	return page;
}

static inline struct page *ppt_get_pd_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *page = info->ppt.pd_base + (gpa >> PDPTE_SHIFT);
	(void)memset(page, 0U, PAGE_SIZE);
	return page;
}

const struct memory_ops ppt_mem_ops = {
	.info = &ppt_pages_info,
	.get_default_access_right = ppt_get_default_access_right,
	.pgentry_present = ppt_pgentry_present,
	.get_pml4_page = ppt_get_pml4_page,
	.get_pdpt_page = ppt_get_pdpt_page,
	.get_pd_page = ppt_get_pd_page,
};

DEFINE_PGTABLE_PAGE(vm0_, pml4, PML4, EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE));
DEFINE_PGTABLE_PAGE(vm0_, pdpt, PDPT, EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE));
DEFINE_PGTABLE_PAGE(vm0_, pd, PD, EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE));
DEFINE_PGTABLE_PAGE(vm0_, pt, PT, EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE));

/* uos_nworld_pml4_pages[i] is ...... of UOS i (whose vm_id = i +1) */
static struct page uos_nworld_pml4_pages[CONFIG_MAX_VM_NUM - 1U][PML4_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
static struct page uos_nworld_pdpt_pages[CONFIG_MAX_VM_NUM - 1U][PDPT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
static struct page uos_nworld_pd_pages[CONFIG_MAX_VM_NUM - 1U][PD_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
static struct page uos_nworld_pt_pages[CONFIG_MAX_VM_NUM - 1U][PT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];

static struct page uos_sworld_pgtable_pages[CONFIG_MAX_VM_NUM - 1U][TRUSTY_PGTABLE_PAGE_NUM(TRUSTY_RAM_SIZE)];

/* ept: extended page table*/
static union pgtable_pages_info ept_pages_info[CONFIG_MAX_VM_NUM] = {
	{
		.ept = {
			.top_address_space = EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE),
			.nworld_pml4_base = vm0_pml4_pages,
			.nworld_pdpt_base = vm0_pdpt_pages,
			.nworld_pd_base = vm0_pd_pages,
			.nworld_pt_base = vm0_pt_pages,
		},
	},
};

static inline uint64_t ept_get_default_access_right(void)
{
	return EPT_RWX;
}

static inline uint64_t ept_pgentry_present(uint64_t pte)
{
	return pte & EPT_RWX;
}

static inline struct page *ept_get_pml4_page(const union pgtable_pages_info *info, __unused uint64_t gpa)
{
	struct page *page = info->ept.nworld_pml4_base;
	(void)memset(page, 0U, PAGE_SIZE);
	return page;
}

static inline struct page *ept_get_pdpt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *page = info->ept.nworld_pdpt_base + (gpa >> PML4E_SHIFT);
	(void)memset(page, 0U, PAGE_SIZE);
	return page;
}

static inline struct page *ept_get_pd_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *page;
	if (gpa < TRUSTY_EPT_REBASE_GPA) {
		page = info->ept.nworld_pd_base + (gpa >> PDPTE_SHIFT);
	} else {
		page = info->ept.sworld_pgtable_base + TRUSTY_PML4_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) +
			TRUSTY_PDPT_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) + ((gpa - TRUSTY_EPT_REBASE_GPA) >> PDPTE_SHIFT);
	}
	(void)memset(page, 0U, PAGE_SIZE);
	return page;
}

static inline struct page *ept_get_pt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *page;
	if (gpa < TRUSTY_EPT_REBASE_GPA) {
		page = info->ept.nworld_pt_base + (gpa >> PDE_SHIFT);
	} else {
		page = info->ept.sworld_pgtable_base + TRUSTY_PML4_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) +
			TRUSTY_PDPT_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) + TRUSTY_PD_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) +
			((gpa - TRUSTY_EPT_REBASE_GPA) >> PDE_SHIFT);
	}
	(void)memset(page, 0U, PAGE_SIZE);
	return page;
}

void init_ept_mem_ops(struct vm *vm)
{
	uint16_t vm_id = vm->vm_id;
	if (vm_id != 0U) {
		ept_pages_info[vm_id].ept.top_address_space = EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE);
		ept_pages_info[vm_id].ept.nworld_pml4_base = uos_nworld_pml4_pages[vm_id - 1U];
		ept_pages_info[vm_id].ept.nworld_pdpt_base = uos_nworld_pdpt_pages[vm_id - 1U];
		ept_pages_info[vm_id].ept.nworld_pd_base = uos_nworld_pd_pages[vm_id - 1U];
		ept_pages_info[vm_id].ept.nworld_pt_base = uos_nworld_pt_pages[vm_id - 1U];
		ept_pages_info[vm_id].ept.sworld_pgtable_base = uos_sworld_pgtable_pages[vm_id - 1U];
	}
	vm->arch_vm.ept_mem_ops.info = &ept_pages_info[vm_id];

	vm->arch_vm.ept_mem_ops.get_default_access_right = ept_get_default_access_right;
	vm->arch_vm.ept_mem_ops.pgentry_present = ept_pgentry_present;
	vm->arch_vm.ept_mem_ops.get_pml4_page = ept_get_pml4_page;
	vm->arch_vm.ept_mem_ops.get_pdpt_page = ept_get_pdpt_page;
	vm->arch_vm.ept_mem_ops.get_pd_page = ept_get_pd_page;
	vm->arch_vm.ept_mem_ops.get_pt_page = ept_get_pt_page;

}
