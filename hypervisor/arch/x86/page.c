/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <rtl.h>
#include <cpufeatures.h>
#include <pgtable.h>
#include <page.h>
#include <mmu.h>
#include <trusty.h>
#include <vtd.h>
#include <security.h>
#include <vm.h>
#include <logmsg.h>


#define MAX_PHY_ADDRESS_SPACE	(1UL << MAXIMUM_PA_WIDTH)

/* PPT VA and PA are identical mapping */
#define PPT_PML4_PAGE_NUM	PML4_PAGE_NUM(MAX_PHY_ADDRESS_SPACE)
#define PPT_PDPT_PAGE_NUM	PDPT_PAGE_NUM(MAX_PHY_ADDRESS_SPACE)
#define PPT_PD_PAGE_NUM	PD_PAGE_NUM(MAX_PHY_ADDRESS_SPACE)
#define PPT_PT_PAGE_NUM	0UL	/* not support 4K granularity page mapping */
/* must be a multiple of 64 */
#define PPT_PAGE_NUM	(roundup((PPT_PML4_PAGE_NUM + PPT_PDPT_PAGE_NUM + \
			PPT_PD_PAGE_NUM + PPT_PT_PAGE_NUM), 64U))
static struct page ppt_pages[PPT_PAGE_NUM];
static uint64_t ppt_page_bitmap[PPT_PAGE_NUM / 64];

/* ppt: pripary page pool */
static struct page_pool ppt_page_pool = {
	.start_page = ppt_pages,
	.bitmap_size = PPT_PAGE_NUM / 64,
	.bitmap = ppt_page_bitmap,
	.last_hint_id = 0UL,
	.dummy_page = NULL,
};

struct page *alloc_page(struct page_pool *pool)
{
	struct page *page = NULL;
	uint64_t loop_idx, idx, bit;

	spinlock_obtain(&pool->lock);
	for (loop_idx = pool->last_hint_id;
		loop_idx < pool->last_hint_id + pool->bitmap_size; loop_idx++) {
		idx = loop_idx % pool->bitmap_size;
		if (*(pool->bitmap + idx) != ~0UL) {
			bit = ffz64(*(pool->bitmap + idx));
			bitmap_set_nolock(bit, pool->bitmap + idx);
			page = pool->start_page + ((idx << 6U) + bit);

			pool->last_hint_id = idx;
			break;
		}
	}
	spinlock_release(&pool->lock);

	ASSERT(page != NULL, "no page aviable!");
	page = (page != NULL) ? page : pool->dummy_page;
	if (page == NULL) {
		/* For HV MMU pagetable mapping, we didn't use dummy page when there's no page
		 * aviable in the page pool. This because we only do MMU pagetable mapping on
		 * the early boot time and we reserve enough pages for it. After that, we would
		 * not do any MMU pagetable mapping. We would let the system boot fail when page
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
	bitmap_clear_nolock(bit, pool->bitmap + idx);
	spinlock_release(&pool->lock);
}

/* @pre: The PPT and EPT have same page granularity */
static inline bool large_page_support(enum _page_table_level level, __unused uint64_t prot)
{
	bool support;

	if (level == IA32E_PD) {
		support = true;
	} else if (level == IA32E_PDPT) {
		support = pcpu_has_vmx_ept_cap(VMX_EPT_1GB_PAGE);
	} else {
		support = false;
	}

	return support;
}

static inline uint64_t ppt_get_default_access_right(void)
{
	return (PAGE_PRESENT | PAGE_RW | PAGE_USER);
}

static inline void ppt_clflush_pagewalk(const void* entry __attribute__((unused)))
{
}

static inline uint64_t ppt_pgentry_present(uint64_t pte)
{
	return pte & PAGE_PRESENT;
}

static inline void nop_tweak_exe_right(uint64_t *entry __attribute__((unused))) {}
static inline void nop_recover_exe_right(uint64_t *entry __attribute__((unused))) {}

const struct memory_ops ppt_mem_ops = {
	.pool = &ppt_page_pool,
	.large_page_support = large_page_support,
	.get_default_access_right = ppt_get_default_access_right,
	.pgentry_present = ppt_pgentry_present,
	.clflush_pagewalk = ppt_clflush_pagewalk,
	.tweak_exe_right = nop_tweak_exe_right,
	.recover_exe_right = nop_recover_exe_right,
};

/* EPT address space will not beyond the platform physical address space */
#define EPT_PML4_PAGE_NUM	PML4_PAGE_NUM(MAX_PHY_ADDRESS_SPACE)
#define EPT_PDPT_PAGE_NUM	PDPT_PAGE_NUM(MAX_PHY_ADDRESS_SPACE)
#define EPT_PD_PAGE_NUM	PD_PAGE_NUM(MAX_PHY_ADDRESS_SPACE)

/* EPT_PT_PAGE_NUM consists of three parts:
 * 1) DRAM - and low MMIO are contiguous (we could assume this because ve820 was build by us),
 *            CONFIG_MAX_VM_NUM at most
 * 2) low MMIO - and DRAM are contiguous, (MEM_1G << 2U) at most
 * 3) high MMIO - Only PCI BARs're high MMIO (we didn't build the high MMIO EPT mapping
 *                except writing PCI 64 bits BARs)
 *
 * The first two parts may use PT_PAGE_NUM(CONFIG_PLATFORM_RAM_SIZE + (MEM_1G << 2U)) PT pages
 * to build EPT mapping at most;
 * The high MMIO may use (CONFIG_MAX_PCI_DEV_NUM * 6U) PT pages to build EPT mapping at most:
 * this is because: (a) each 64 bits MMIO BAR may spend one PT page at most to build EPT mapping,
 *                      MMIO BAR size must be a power of 2 from 16 bytes;
 *                      MMIO BAR base address must be power of two in size and are aligned with its size;
 *                      So if the MMIO BAR size is less than 2M, one PT page is enough to cover its EPT mapping,
 *                      if the MMIO size is larger than 2M, it must be multiple of 2M, we could use large pages
 *                      to build EPT mapping for it. The single exception is fliter the MSI-X structure part
 *                      from the MSI-X table BAR. In this case, it will also spend one PT page.
 *                  (b) each PCI device may have six 64 bits MMIO (three general BARs plus three VF BARs)
 *                  (c) The Maximum number of PCI devices for ACRN and the Maximum number of virtual PCI devices
 *                      for VM both are CONFIG_PLATFORM_RAM_SIZE
 */
#define EPT_PT_PAGE_NUM	(PT_PAGE_NUM(CONFIG_PLATFORM_RAM_SIZE + (MEM_1G << 2U)) + \
			CONFIG_MAX_PCI_DEV_NUM * 6U)

/* must be a multiple of 64 */
#define EPT_PAGE_NUM	(roundup((EPT_PML4_PAGE_NUM + EPT_PDPT_PAGE_NUM + \
			EPT_PD_PAGE_NUM + EPT_PT_PAGE_NUM), 64U))
#define TOTAL_EPT_4K_PAGES_SIZE (CONFIG_MAX_VM_NUM * (EPT_PAGE_NUM) * PAGE_SIZE)

static struct page *ept_pages[CONFIG_MAX_VM_NUM];
static uint64_t ept_page_bitmap[CONFIG_MAX_VM_NUM][EPT_PAGE_NUM / 64];
static struct page ept_dummy_pages[CONFIG_MAX_VM_NUM];

/* ept: extended page pool*/
static struct page_pool ept_page_pool[CONFIG_MAX_VM_NUM];

/* pre-assumption: TRUSTY_RAM_SIZE is 2M aligned */
static struct page post_uos_sworld_memory[MAX_POST_VM_NUM][TRUSTY_RAM_SIZE >> PAGE_SHIFT] __aligned(MEM_2M);



/*
 * @brief Reserve space for EPT 4K pages from platform E820 table
 */
void reserve_buffer_for_ept_pages(void)
{
	uint64_t page_base;
	uint16_t vm_id;
	uint32_t offset = 0U;

	page_base = e820_alloc_memory(TOTAL_EPT_4K_PAGES_SIZE, ~0UL);
	ppt_clear_user_bit(page_base, TOTAL_EPT_4K_PAGES_SIZE);
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		ept_pages[vm_id] = (struct page *)(void *)(page_base + offset);
		/* assume each VM has same amount of EPT pages */
		offset += EPT_PAGE_NUM * PAGE_SIZE;
	}
}

void *get_reserve_sworld_memory_base(void)
{
	return post_uos_sworld_memory;
}

/*
 * Pages without execution right, such as MMIO, can always use large page
 * base on hardware capability, even if the VM is an RTVM. This can save
 * page table page # and improve TLB hit rate.
 */
static inline bool use_large_page(enum _page_table_level level, uint64_t prot)
{
	bool ret = false;	/* for code page */

	if ((prot & EPT_EXE) == 0UL) {
		ret = large_page_support(level, prot);
	}

	return ret;
}

static inline uint64_t ept_get_default_access_right(void)
{
	return EPT_RWX;
}

static inline uint64_t ept_pgentry_present(uint64_t pte)
{
	return pte & EPT_RWX;
}

static inline void ept_clflush_pagewalk(const void* etry)
{
	iommu_flush_cache(etry, sizeof(uint64_t));
}

/* The function is used to disable execute right for (2MB / 1GB)large pages in EPT */
static inline void ept_tweak_exe_right(uint64_t *entry)
{
	*entry &= ~EPT_EXE;
}

/* The function is used to recover the execute right when large pages are breaking into 4KB pages
 * Hypervisor doesn't control execute right for guest memory, recovers execute right by default.
 */
static inline void ept_recover_exe_right(uint64_t *entry)
{
	*entry |= EPT_EXE;
}

void init_ept_mem_ops(struct memory_ops *mem_ops, uint16_t vm_id)
{
	struct acrn_vm *vm = get_vm_from_vmid(vm_id);

	ept_page_pool[vm_id].start_page = ept_pages[vm_id];
	ept_page_pool[vm_id].bitmap_size = EPT_PAGE_NUM / 64;
	ept_page_pool[vm_id].bitmap = ept_page_bitmap[vm_id];
	ept_page_pool[vm_id].dummy_page = &ept_dummy_pages[vm_id];

	spinlock_init(&ept_page_pool[vm_id].lock);
	memset((void *)ept_page_pool[vm_id].bitmap, 0, ept_page_pool[vm_id].bitmap_size * sizeof(uint64_t));
	ept_page_pool[vm_id].last_hint_id = 0UL;

	if (is_postlaunched_vm(vm)) {
		uint16_t sos_vm_id = (get_sos_vm())->vm_id;
		uint16_t page_idx = vmid_2_rel_vmid(sos_vm_id, vm_id) - 1U;

		vm->arch_vm.sworld_memory_base_hva = post_uos_sworld_memory[page_idx];
	}

	mem_ops->pool = &ept_page_pool[vm_id];
	mem_ops->get_default_access_right = ept_get_default_access_right;
	mem_ops->pgentry_present = ept_pgentry_present;
	mem_ops->clflush_pagewalk = ept_clflush_pagewalk;
	mem_ops->large_page_support = large_page_support;

	/* Mitigation for issue "Machine Check Error on Page Size Change" */
	if (is_ept_force_4k_ipage()) {
		mem_ops->tweak_exe_right = ept_tweak_exe_right;
		mem_ops->recover_exe_right = ept_recover_exe_right;
		/* For RTVM, build 4KB page mapping in EPT for code pages */
		if (is_rt_vm(vm)) {
			mem_ops->large_page_support = use_large_page;
		}
	} else {
		mem_ops->tweak_exe_right = nop_tweak_exe_right;
		mem_ops->recover_exe_right = nop_recover_exe_right;
	}
}
