/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PAGE_H
#define PAGE_H

#include <board_info.h>

#define PAGE_SHIFT	12U
#define PAGE_SIZE	(1U << PAGE_SHIFT)
#define PAGE_MASK	0xFFFFFFFFFFFFF000UL

/* size of the low MMIO address space: 2GB */
#define PLATFORM_LO_MMIO_SIZE	0x80000000UL

/* size of the high MMIO address space: 1GB */
#define PLATFORM_HI_MMIO_SIZE	0x40000000UL

#define PML4_PAGE_NUM(size)	1UL
#define PDPT_PAGE_NUM(size)	(((size) + PML4E_SIZE - 1UL) >> PML4E_SHIFT)
#define PD_PAGE_NUM(size)	(((size) + PDPTE_SIZE - 1UL) >> PDPTE_SHIFT)
#define PT_PAGE_NUM(size)	(((size) + PDE_SIZE - 1UL) >> PDE_SHIFT)

/*
 * The size of the guest physical address space, covered by the EPT page table of a VM.
 * With the assumptions:
 * - The GPA of DRAM & MMIO are contiguous.
 * - Guest OS won't re-program device MMIO bars to the address not covered by
 *   this EPT_ADDRESS_SPACE.
 */
#define EPT_ADDRESS_SPACE(size)		(((size) > MEM_2G) ?	\
			((size) + PLATFORM_LO_MMIO_SIZE + PLATFORM_HI_MMIO_SIZE)	\
			: (MEM_2G + PLATFORM_LO_MMIO_SIZE + PLATFORM_HI_MMIO_SIZE))

#define PTDEV_HI_MMIO_START		((CONFIG_UOS_RAM_SIZE > MEM_2G) ?	\
			(CONFIG_UOS_RAM_SIZE + PLATFORM_LO_MMIO_SIZE) : (MEM_2G + PLATFORM_LO_MMIO_SIZE))

#define PRE_VM_EPT_ADDRESS_SPACE(size)	(PTDEV_HI_MMIO_START + HI_MMIO_SIZE)

#define TOTAL_EPT_4K_PAGES_SIZE		(PRE_VM_NUM*(PT_PAGE_NUM(PRE_VM_EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))*MEM_4K)) + \
						(SOS_VM_NUM*(PT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE))*MEM_4K)) + \
						(MAX_POST_VM_NUM*(PT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))*MEM_4K))

#define TRUSTY_PML4_PAGE_NUM(size)	(1UL)
#define TRUSTY_PDPT_PAGE_NUM(size)	(1UL)
#define TRUSTY_PD_PAGE_NUM(size)	(PD_PAGE_NUM(size))
#define TRUSTY_PT_PAGE_NUM(size)	(PT_PAGE_NUM(size))
#define TRUSTY_PGTABLE_PAGE_NUM(size)	\
(TRUSTY_PML4_PAGE_NUM(size) + TRUSTY_PDPT_PAGE_NUM(size) + TRUSTY_PD_PAGE_NUM(size) + TRUSTY_PT_PAGE_NUM(size))

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
	bool (*large_page_support)(enum _page_table_level level);
	uint64_t (*get_default_access_right)(void);
	uint64_t (*pgentry_present)(uint64_t pte);
	struct page *(*get_pml4_page)(const union pgtable_pages_info *info);
	struct page *(*get_pdpt_page)(const union pgtable_pages_info *info, uint64_t gpa);
	struct page *(*get_pd_page)(const union pgtable_pages_info *info, uint64_t gpa);
	struct page *(*get_pt_page)(const union pgtable_pages_info *info, uint64_t gpa);
	void *(*get_sworld_memory_base)(const union pgtable_pages_info *info);
	void (*clflush_pagewalk)(const void *p);
	void (*tweak_exe_right)(uint64_t *entry);
	void (*recover_exe_right)(uint64_t *entry);
};

extern const struct memory_ops ppt_mem_ops;
void init_ept_mem_ops(struct memory_ops *mem_ops, uint16_t vm_id);
void *get_reserve_sworld_memory_base(void);

#ifdef CONFIG_LAST_LEVEL_EPT_AT_BOOT
void reserve_buffer_for_ept_pages(void);
#endif
#endif /* PAGE_H */
