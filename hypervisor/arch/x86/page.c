/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <bits.h>
#include <asm/page.h>
#include <logmsg.h>

/**
 * @addtogroup hwmgmt_page
 *
 * @{
 */

/**
 * @file
 * @brief Implementation of page management.
 *
 * This file provides the core functionality required for allocating and freeing memory pages. It's a fundamental
 * support to manage memory resources.
 */

struct page *alloc_page(struct page_pool *pool)
{
	struct page *page = NULL;
	uint64_t loop_idx, idx, bit;

	spinlock_obtain(&pool->lock);
	for (loop_idx = pool->last_hint_id;
		loop_idx < (pool->last_hint_id + pool->bitmap_size); loop_idx++) {
		idx = loop_idx % pool->bitmap_size;
		if (*(pool->bitmap + idx) != ~0UL) {
			bit = ffz64(*(pool->bitmap + idx));
			bitmap_set_non_atomic(bit, pool->bitmap + idx);
			page = pool->start_page + ((idx << 6U) + bit);

			pool->last_hint_id = idx;
			break;
		}
	}
	spinlock_release(&pool->lock);

	ASSERT(page != NULL, "no page aviable!");
	page = (page != NULL) ? page : pool->dummy_page;
	if (page == NULL) {
		/* For HV MMU page-table mapping, we didn't use dummy page when there's no page
		 * available in the page pool. This because we only do MMU page-table mapping on
		 * the early boot time and we reserve enough pages for it. After that, we would
		 * not do any MMU page-table mapping. We would let the system boot fail when page
		 * allocation failed.
		 */
		panic("no dummy aviable!");
	}
	(void)memset(page, 0U, PAGE_SIZE);
	return page;
}

/*
 *@pre: ((page - pool->start_page) >> 6U) < pool->bitmap_size
 */
void free_page(struct page_pool *pool, struct page *page)
{
	uint64_t idx, bit;

	spinlock_obtain(&pool->lock);
	idx = (page - pool->start_page) >> 6U;
	bit = (page - pool->start_page) & 0x3fUL;
	bitmap_clear_non_atomic(bit, pool->bitmap + idx);
	spinlock_release(&pool->lock);
}

/**
 * @}
 */