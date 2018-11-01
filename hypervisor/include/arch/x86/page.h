/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PAGE_H
#define PAGE_H

#define PAGE_SHIFT	12U
#define PAGE_SIZE	(1UL << PAGE_SHIFT)

/* size of the low MMIO address space: 2GB */
#define PLATFORM_LO_MMIO_SIZE	0x80000000UL

struct page {
	uint8_t contents[PAGE_SIZE];
} __aligned(PAGE_SIZE);

union pgtable_pages_info {
	struct {
		struct page *pml4_base;
		struct page *pdpt_base;
		struct page *pd_base;
		struct page *pt_base;
	} ppt;
	struct {
		uint64_t top_address_space;
		struct page *nworld_pml4_base;
		struct page *nworld_pdpt_base;
		struct page *nworld_pd_base;
		struct page *nworld_pt_base;
		struct page *sworld_pgtable_base;
	} ept;
};

struct memory_ops {
	union pgtable_pages_info *info;
	uint64_t (*get_default_access_right)(void);
	uint64_t (*pgentry_present)(uint64_t pte);
	struct page *(*get_pml4_page)(const union pgtable_pages_info *info, uint64_t gpa);
	struct page *(*get_pdpt_page)(const union pgtable_pages_info *info, uint64_t gpa);
	struct page *(*get_pd_page)(const union pgtable_pages_info *info, uint64_t gpa);
	struct page *(*get_pt_page)(const union pgtable_pages_info *info, uint64_t gpa);
};

extern const struct memory_ops ppt_mem_ops;
void init_ept_mem_ops(struct vm *vm);

#endif /* PAGE_H */
