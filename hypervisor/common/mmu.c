/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <lib/bits.h>
#include <asm/page.h>
#include <logmsg.h>
#include <util.h>
#include <mmu.h>
#include <asm/init.h>
#include <pgtable.h>
#include <acrn_hv_defs.h>


void init_page_pool(struct page_pool *pool, uint64_t *page_base, uint64_t *bitmap_base, int page_num)
{
       uint64_t bitmap_size = page_num / 8;
       pool->bitmap = (uint64_t *)bitmap_base;
       pool->start_page = (struct page *)page_base;
       pool->bitmap_size = bitmap_size / sizeof(uint64_t);
       pool->dummy_page = NULL;

       memset(pool->bitmap, 0, bitmap_size);
}

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

static uint64_t sanitized_page_hpa;

void sanitize_pte_entry(uint64_t *ptep, const struct pgtable *table)
{
	set_pgentry(ptep, sanitized_page_hpa, table);
}

void sanitize_pte(uint64_t *pt_page, const struct pgtable *table)
{
	uint64_t i;
	for (i = 0UL; i < PTRS_PER_PTE; i++) {
		sanitize_pte_entry(pt_page + i, table);
	}
}

/**
 * For x86, sanitized_page_hpa need point to one specific page,
 * for other arch,  sanitized_page_hpa is by default 0 without
 * calling this function.
 */
void init_sanitized_page(uint64_t *sanitized_page, uint64_t hpa)
{
	uint64_t i;

	sanitized_page_hpa = hpa;
	/* set ptep in sanitized_page point to itself */
	for (i = 0UL; i < PTRS_PER_PTE; i++) {
		*(sanitized_page + i) = sanitized_page_hpa;
	}
}
