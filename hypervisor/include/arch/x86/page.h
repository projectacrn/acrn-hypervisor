/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PAGE_H
#define PAGE_H

#define PAGE_SHIFT	12U
#define PAGE_SIZE	(1U << PAGE_SHIFT)

/* size of the low MMIO address space: 2GB */
#define PLATFORM_LO_MMIO_SIZE	0x80000000UL

#define PML4_PAGE_NUM(size)	1UL
#define PDPT_PAGE_NUM(size)	(((size) + PML4E_SIZE - 1UL) >> PML4E_SHIFT)
#define PD_PAGE_NUM(size)	(((size) + PDPTE_SIZE - 1UL) >> PDPTE_SHIFT)
#define PT_PAGE_NUM(size)	(((size) + PDE_SIZE - 1UL) >> PDE_SHIFT)

/* The size of the guest physical address space, covered by the EPT page table of a VM */
#define EPT_ADDRESS_SPACE(size)	((size != 0UL) ? (size + PLATFORM_LO_MMIO_SIZE) : 0UL)

#define TRUSTY_PML4_PAGE_NUM(size)	(1UL)
#define TRUSTY_PDPT_PAGE_NUM(size)	(1UL)
#define TRUSTY_PD_PAGE_NUM(size)	(PD_PAGE_NUM(size))
#define TRUSTY_PT_PAGE_NUM(size)	(PT_PAGE_NUM(size))
#define TRUSTY_PGTABLE_PAGE_NUM(size)	\
(TRUSTY_PML4_PAGE_NUM(size) + TRUSTY_PDPT_PAGE_NUM(size) + TRUSTY_PD_PAGE_NUM(size) + TRUSTY_PT_PAGE_NUM(size))

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
		struct page *sworld_memory_base;
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
	void *(*get_sworld_memory_base)(const union pgtable_pages_info *info);
};

extern const struct memory_ops ppt_mem_ops;
void init_ept_mem_ops(struct acrn_vm *vm);
void *get_reserve_sworld_memory_base(void);

#endif /* PAGE_H */
