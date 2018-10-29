/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <hypervisor.h>

#define PML4_PAGE_NUM(size)	1UL
#define PDPT_PAGE_NUM(size)	(((size) + PML4E_SIZE - 1UL) >> PML4E_SHIFT)
#define PD_PAGE_NUM(size)	(((size) + PDPTE_SIZE - 1UL) >> PDPTE_SHIFT)
#define PT_PAGE_NUM(size)	(((size) + PDE_SIZE - 1UL) >> PDE_SHIFT)

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
