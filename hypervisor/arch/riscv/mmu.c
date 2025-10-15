/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <rtl.h>
#include <mmu.h>
#include <asm/qemu.h>

void set_paging_supervisor(__unused uint64_t base, __unused uint64_t size)
{
}

static struct page_pool ppt_page_pool;

static bool large_page_support(enum _page_table_level level, uint64_t __unused prot)
{
	if (level == PGT_LVL1|| level == PGT_LVL2)
		return true;
	else
		return false;
}

static void ppt_flush_cache_pagewalk(const void* entry __attribute__((unused)))
{
}

static uint64_t ppt_pgentry_present(uint64_t pte)
{
	return pte & PAGE_V;
}

static inline void ppt_set_pgentry(uint64_t *pte, uint64_t page, uint64_t prot, enum _page_table_level __unused level,
		bool is_leaf, const struct pgtable *table)
{
	uint64_t prot_tmp;
	if (!is_leaf) {
		prot_tmp = PAGE_V;
	} else {
		prot_tmp = prot;
	}
	make_pgentry(pte, page, prot_tmp, table);
}

static const struct pgtable ppt_pgtable = {
	.pool = &ppt_page_pool,
	.large_page_support = large_page_support,
	.pgentry_present = ppt_pgentry_present,
	.flush_cache_pagewalk = ppt_flush_cache_pagewalk,
	.set_pgentry = ppt_set_pgentry,
};
