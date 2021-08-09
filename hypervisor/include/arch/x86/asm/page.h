/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PAGE_H
#define PAGE_H

#include <asm/lib/spinlock.h>
#include <board_info.h>

#define PAGE_SHIFT	12U
#define PAGE_SIZE	(1U << PAGE_SHIFT)
#define PAGE_MASK	0xFFFFFFFFFFFFF000UL

#define MAX_PHY_ADDRESS_SPACE	(1UL << MAXIMUM_PA_WIDTH)

/* size of the low MMIO address space: 2GB */
#define PLATFORM_LO_MMIO_SIZE	0x80000000UL

/* size of the high MMIO address space: 1GB */
#define PLATFORM_HI_MMIO_SIZE	0x40000000UL

#define PML4_PAGE_NUM(size)	1UL
#define PDPT_PAGE_NUM(size)	(((size) + PML4E_SIZE - 1UL) >> PML4E_SHIFT)
#define PD_PAGE_NUM(size)	(((size) + PDPTE_SIZE - 1UL) >> PDPTE_SHIFT)
#define PT_PAGE_NUM(size)	(((size) + PDE_SIZE - 1UL) >> PDE_SHIFT)


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

struct page *alloc_page(struct page_pool *pool);
void free_page(struct page_pool *pool, struct page *page);
#endif /* PAGE_H */
