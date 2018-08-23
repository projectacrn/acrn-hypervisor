/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PGTABLE_TYPES_H
#define PGTABLE_TYPES_H

#define PAGE_PRESENT		(1UL << 0U)
#define PAGE_RW			(1UL << 1U)
#define PAGE_USER		(1UL << 2U)
#define PAGE_PWT		(1UL << 3U)
#define PAGE_PCD		(1UL << 4U)
#define PAGE_ACCESSED		(1UL << 5U)
#define PAGE_DIRTY		(1UL << 6U)
#define PAGE_PSE		(1UL << 7U)
#define PAGE_GLOBAL		(1UL << 8U)
#define PAGE_PAT_LARGE		(1UL << 12U)
#define PAGE_NX			(1UL << 63U)

#define PAGE_CACHE_MASK		(PAGE_PCD | PAGE_PWT)
#define PAGE_CACHE_WB		0UL
#define PAGE_CACHE_WT		PAGE_PWT
#define PAGE_CACHE_UC_MINUS	PAGE_PCD
#define PAGE_CACHE_UC		(PAGE_PCD | PAGE_PWT)

#define PAGE_TABLE		(PAGE_PRESENT | PAGE_RW | PAGE_USER)


#define EPT_RD			(1UL << 0U)
#define EPT_WR			(1UL << 1U)
#define EPT_EXE			(1UL << 2U)
#define EPT_MT_SHIFT		3U
#define EPT_UNCACHED		(0UL << EPT_MT_SHIFT)
#define EPT_WC			(1UL << EPT_MT_SHIFT)
#define EPT_WT			(4UL << EPT_MT_SHIFT)
#define EPT_WP			(5UL << EPT_MT_SHIFT)
#define EPT_WB			(6UL << EPT_MT_SHIFT)
#define EPT_MT_MASK		(7UL << EPT_MT_SHIFT)
/* VTD: Second-Level Paging Entries: Snoop Control */
#define EPT_SNOOP_CTRL		(1UL << 11U)
#define EPT_VE			(1UL << 63U)

#define EPT_RWX			(EPT_RD | EPT_WR | EPT_EXE)


#define PML4E_SHIFT		39U
#define PTRS_PER_PML4E		512UL
#define PML4E_SIZE		(1UL << PML4E_SHIFT)
#define PML4E_MASK		(~(PML4E_SIZE - 1UL))

#define PDPTE_SHIFT		30U
#define PTRS_PER_PDPTE		512UL
#define PDPTE_SIZE		(1UL << PDPTE_SHIFT)
#define PDPTE_MASK		(~(PDPTE_SIZE - 1UL))

#define PDE_SHIFT		21U
#define PTRS_PER_PDE		512UL
#define PDE_SIZE		(1UL << PDE_SHIFT)
#define PDE_MASK		(~(PDE_SIZE - 1UL))

#define PTE_SHIFT		12U
#define PTRS_PER_PTE		512UL
#define PTE_SIZE		(1UL << PTE_SHIFT)
#define PTE_MASK		(~(PTE_SIZE - 1UL))

/* TODO: PAGE_MASK & PHYSICAL_MASK */
#define PML4E_PFN_MASK		0x0000FFFFFFFFF000UL
#define PDPTE_PFN_MASK		0x0000FFFFFFFFF000UL
#define PDE_PFN_MASK		0x0000FFFFFFFFF000UL

#endif /* PGTABLE_TYPES_H */
