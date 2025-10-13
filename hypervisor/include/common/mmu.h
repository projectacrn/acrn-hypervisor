/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MMU_H
#define MMU_H

#include <lib/spinlock.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>

void set_paging_supervisor(uint64_t base, uint64_t size);

/**
 * @brief Data structure that contains a pool of memory pages.
 *
 * This structure is designed to manage a collection of memory pages, facilitating efficient allocation,
 * deallocation, and reuse of pages. It is typically used in scenarios where memory allocation performance
 * is critical, such as in operating systems or high-performance applications. The page pool aims to minimize
 * the overhead associated with frequent memory page allocations by maintaining a ready-to-use pool of pages.
 * It is used to support the memory management in hypervisor and the extended page-table mechanism for VMs.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
struct page_pool {
        struct page *start_page; /**< The first page in the pool. */
        spinlock_t lock; /**< The spinlock to protect simultaneous access of the page pool. */
        /**
         * @brief A pointer to the bitmap that represents the allocation status of each page in the pool.
         *
         * The bitmap is a data structure that represents the allocation status of each page in the pool. Each bit in
         * the bitmap corresponds to a page in the pool. If the bit is set to 1, the page is allocated; otherwise, the
         * page is free. The bitmap is used to track the allocation status of each page in the pool.
         */
        uint64_t *bitmap;
        uint64_t bitmap_size; /**< The number of bitmap. */
        uint64_t last_hint_id; /**< The last bitmap ID that is used to allocate a page. */
        /**
         * @brief A pointer to the dummy page
         *
         * This is used when there's no page available in the pool.
         */
        struct page *dummy_page;
};

/**
 * @brief Page tables level in paging mode
 *
 * 4-level paging may map addresses to 4-KByte pages, 2-MByte pages, or 1-GByte pages. The 4 levels
 * are PGT_LVL3, PGT_LVL2, PGT_LVL1, and PGT_LVL0. The value to present each level is fixed.
 * the mapping to specific architecture's 4 level paging stage is as below:
 *
 *	    x86                                    riscv
 * PGT_LVL3   Page-Map-Level-4(PML4)                 vpn3
 * PGT_LVL2   Page-Directory-Pointer-Table(PDPT)     vpn2
 * PGT_LVL1   Page-Directory(PD)                     vpn1
 * PGT_LVL0   Page-Table(PT)                         vpn0
 */
enum _page_table_level {
	PGT_LVL3 = 0,
	PGT_LVL2 = 1,
	PGT_LVL1 = 2,
	PGT_LVL0 = 3,
};

struct pgtable {
	struct page_pool *pool;
	uint64_t (*get_default_access_right)(void);
	uint64_t (*pgentry_present)(uint64_t pte);
	bool (*large_page_support)(enum _page_table_level level, uint64_t prot);
	void (*flush_cache_pagewalk)(const void *p);
	void (*tweak_exe_right)(uint64_t *entry);
	void (*recover_exe_right)(uint64_t *entry);
};

/*
 * pgentry may means generic page table entry
 */
static inline uint64_t get_pgentry(const uint64_t *pte)
{
	return *pte;
}

/*
 * pgentry may means generic page table entry
 */
static inline void set_pgentry(uint64_t *ptep, uint64_t pte, const struct pgtable *table)
{
	*ptep = pte;
	if (table && table->flush_cache_pagewalk)
		table->flush_cache_pagewalk(ptep);
}

void init_page_pool(struct page_pool *pool, uint64_t *page_base,
		uint64_t *bitmap_base, int page_num);
struct page *alloc_page(struct page_pool *pool);
void free_page(struct page_pool *pool, struct page *page);
void init_sanitized_page(uint64_t *sanitized_page, uint64_t hpa);
void sanitize_pte_entry(uint64_t *ptep, const struct pgtable *table);
void sanitize_pte(uint64_t *pt_page, const struct pgtable *table);
void *pgtable_create_root(const struct pgtable *table);
const uint64_t *pgtable_lookup_entry(uint64_t *pgtl3_page, uint64_t addr,
               uint64_t *pg_size, const struct pgtable *table);

void pgtable_add_map(uint64_t *pgtl3_page, uint64_t paddr_base,
               uint64_t vaddr_base, uint64_t size,
               uint64_t prot, const struct pgtable *table);
void pgtable_modify_or_del_map(uint64_t *pgtl3_page, uint64_t vaddr_base,
               uint64_t size, uint64_t prot_set, uint64_t prot_clr,
               const struct pgtable *table, uint32_t type);

#endif /* MMU_H */
