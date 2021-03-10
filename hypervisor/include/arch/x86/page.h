/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PAGE_H
#define PAGE_H

#include <spinlock.h>
#include <board_info.h>

#define PAGE_SHIFT	12U
#define PAGE_SIZE	(1U << PAGE_SHIFT)
#define PAGE_MASK	0xFFFFFFFFFFFFF000UL

#define MAXIMUM_PA_WIDTH	39U	/* maximum physical-address width */
#define MAX_PHY_ADDRESS_SPACE	(1UL << MAXIMUM_PA_WIDTH)

/* size of the low MMIO address space: 2GB */
#define PLATFORM_LO_MMIO_SIZE	0x80000000UL

/* size of the high MMIO address space: 1GB */
#define PLATFORM_HI_MMIO_SIZE	0x40000000UL

#define PML4_PAGE_NUM(size)	1UL
#define PDPT_PAGE_NUM(size)	(((size) + PML4E_SIZE - 1UL) >> PML4E_SHIFT)
#define PD_PAGE_NUM(size)	(((size) + PDPTE_SIZE - 1UL) >> PDPTE_SHIFT)
#define PT_PAGE_NUM(size)	(((size) + PDE_SIZE - 1UL) >> PDE_SHIFT)


/**
 * @brief Page tables level in IA32 paging mode
 */
enum _page_table_level {
        /**
         * @brief The PML4 level in the page tables
         */
	IA32E_PML4 = 0,
        /**
         * @brief The Page-Directory-Pointer-Table level in the page tables
         */
	IA32E_PDPT = 1,
        /**
         * @brief The Page-Directory level in the page tables
         */
	IA32E_PD = 2,
        /**
         * @brief The Page-Table level in the page tables
         */
	IA32E_PT = 3,
};

struct acrn_vm;

struct page {
	uint8_t contents[PAGE_SIZE];
} __aligned(PAGE_SIZE);

struct page_pool {
	struct page *start_page;
	spinlock_t lock;
	uint64_t bitmap_size;
	uint64_t *bitmap;
	uint64_t last_hint_id;

	struct page *dummy_page;
};

struct pgtable {
	uint64_t default_access_right;
	struct page_pool *pool;
	bool (*large_page_support)(enum _page_table_level level, uint64_t prot);
	uint64_t (*pgentry_present)(uint64_t pte);
	void (*clflush_pagewalk)(const void *p);
	void (*tweak_exe_right)(uint64_t *entry);
	void (*recover_exe_right)(uint64_t *entry);
};

struct page *alloc_page(struct page_pool *pool);
void free_page(struct page_pool *pool, struct page *page);
#endif /* PAGE_H */
