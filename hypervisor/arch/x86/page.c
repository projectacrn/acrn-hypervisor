/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <rtl.h>
#include <x86/cpufeatures.h>
#include <x86/pgtable.h>
#include <x86/page.h>
#include <x86/mmu.h>
#include <x86/guest/trusty.h>
#include <x86/vtd.h>
#include <x86/security.h>
#include <x86/guest/vm.h>

#define LINEAR_ADDRESS_SPACE_48_BIT	(1UL << 48U)

static struct page ppt_pml4_pages[PML4_PAGE_NUM(LINEAR_ADDRESS_SPACE_48_BIT)];
static struct page ppt_pdpt_pages[PDPT_PAGE_NUM(LINEAR_ADDRESS_SPACE_48_BIT)];
static struct page ppt_pd_pages[PD_PAGE_NUM(CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE)];

/* ppt: pripary page table */
static union pgtable_pages_info ppt_pages_info = {
	.ppt = {
		.pml4_base = ppt_pml4_pages,
		.pdpt_base = ppt_pdpt_pages,
		.pd_base = ppt_pd_pages,
	}
};

/* @pre: The PPT and EPT have same page granularity */
static inline bool large_page_support(enum _page_table_level level)
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

static inline struct page *ppt_get_pml4_page(const union pgtable_pages_info *info)
{
	struct page *pml4_page = info->ppt.pml4_base;
	(void)memset(pml4_page, 0U, PAGE_SIZE);
	return pml4_page;
}

static inline struct page *ppt_get_pdpt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *pdpt_page = info->ppt.pdpt_base + (gpa >> PML4E_SHIFT);
	(void)memset(pdpt_page, 0U, PAGE_SIZE);
	return pdpt_page;
}

static inline struct page *ppt_get_pd_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *pd_page = info->ppt.pd_base + (gpa >> PDPTE_SHIFT);
	(void)memset(pd_page, 0U, PAGE_SIZE);
	return pd_page;
}

static inline void nop_tweak_exe_right(uint64_t *entry __attribute__((unused))) {}
static inline void nop_recover_exe_right(uint64_t *entry __attribute__((unused))) {}

const struct memory_ops ppt_mem_ops = {
	.info = &ppt_pages_info,
	.large_page_support = large_page_support,
	.get_default_access_right = ppt_get_default_access_right,
	.pgentry_present = ppt_pgentry_present,
	.get_pml4_page = ppt_get_pml4_page,
	.get_pdpt_page = ppt_get_pdpt_page,
	.get_pd_page = ppt_get_pd_page,
	.clflush_pagewalk = ppt_clflush_pagewalk,
	.tweak_exe_right = nop_tweak_exe_right,
	.recover_exe_right = nop_recover_exe_right,
};

static struct page sos_vm_pml4_pages[SOS_VM_NUM][PML4_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE))];
static struct page sos_vm_pdpt_pages[SOS_VM_NUM][PDPT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE))];
static struct page sos_vm_pd_pages[SOS_VM_NUM][PD_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE))];
/* pre_uos_nworld_pml4_pages */
static struct page pre_uos_nworld_pml4_pages[PRE_VM_NUM][PML4_PAGE_NUM(PRE_VM_EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
static struct page pre_uos_nworld_pdpt_pages[PRE_VM_NUM][PDPT_PAGE_NUM(PRE_VM_EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
static struct page pre_uos_nworld_pd_pages[PRE_VM_NUM][PD_PAGE_NUM(PRE_VM_EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];

/* post_uos_nworld_pml4_pages */
static struct page post_uos_nworld_pml4_pages[MAX_POST_VM_NUM][PML4_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
static struct page post_uos_nworld_pdpt_pages[MAX_POST_VM_NUM][PDPT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
static struct page post_uos_nworld_pd_pages[MAX_POST_VM_NUM][PD_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];

static struct page post_uos_sworld_pgtable_pages[MAX_POST_VM_NUM][TRUSTY_PGTABLE_PAGE_NUM(TRUSTY_RAM_SIZE)];
/* pre-assumption: TRUSTY_RAM_SIZE is 2M aligned */
static struct page post_uos_sworld_memory[MAX_POST_VM_NUM][TRUSTY_RAM_SIZE >> PAGE_SHIFT] __aligned(MEM_2M);

/* ept: extended page table*/
static union pgtable_pages_info ept_pages_info[CONFIG_MAX_VM_NUM];


#ifdef CONFIG_LAST_LEVEL_EPT_AT_BOOT
/* Array with address space size for each type of load order of VM */
static const uint64_t vm_address_space_size[MAX_LOAD_ORDER] = {
	PRE_VM_EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE), /* for Pre-Launched VM */
	EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE), /* for SOS VM */
	EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE), /* for Post-Launched VM */
};

/*
 * @brief Reserve space for EPT 4K pages from platform E820 table
 */
void reserve_buffer_for_ept_pages(void)
{
	uint64_t pt_base;
	uint16_t vm_id;
	uint32_t offset = 0U;
	struct acrn_vm_config *vm_config;

	pt_base = e820_alloc_memory(TOTAL_EPT_4K_PAGES_SIZE, ~0UL);
	ppt_clear_user_bit(pt_base, TOTAL_EPT_4K_PAGES_SIZE);
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		ept_pages_info[vm_id].ept.nworld_pt_base = (struct page *)(void *)(pt_base + offset);
		offset += PT_PAGE_NUM(vm_address_space_size[vm_config->load_order])*MEM_4K;
	}
}
#else
static struct page sos_vm_pt_pages[SOS_VM_NUM][PT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE))];
static struct page pre_uos_nworld_pt_pages[PRE_VM_NUM][PT_PAGE_NUM(PRE_VM_EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
static struct page post_uos_nworld_pt_pages[MAX_POST_VM_NUM][PT_PAGE_NUM(EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE))];
#endif

void *get_reserve_sworld_memory_base(void)
{
	return post_uos_sworld_memory;
}

static inline bool large_page_not_support(__unused enum _page_table_level level)
{
	return false;
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

static inline struct page *ept_get_pml4_page(const union pgtable_pages_info *info)
{
	struct page *pml4_page = info->ept.nworld_pml4_base;
	(void)memset(pml4_page, 0U, PAGE_SIZE);
	return pml4_page;
}

static inline struct page *ept_get_pdpt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *pdpt_page = info->ept.nworld_pdpt_base + (gpa >> PML4E_SHIFT);
	(void)memset(pdpt_page, 0U, PAGE_SIZE);
	return pdpt_page;
}

static inline struct page *ept_get_pd_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *pd_page;
	if (gpa < TRUSTY_EPT_REBASE_GPA) {
		pd_page = info->ept.nworld_pd_base + (gpa >> PDPTE_SHIFT);
	} else {
		pd_page = info->ept.sworld_pgtable_base + TRUSTY_PML4_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) +
			TRUSTY_PDPT_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) + ((gpa - TRUSTY_EPT_REBASE_GPA) >> PDPTE_SHIFT);
	}
	(void)memset(pd_page, 0U, PAGE_SIZE);
	return pd_page;
}

static inline struct page *ept_get_pt_page(const union pgtable_pages_info *info, uint64_t gpa)
{
	struct page *pt_page;
	if (gpa < TRUSTY_EPT_REBASE_GPA) {
		pt_page = info->ept.nworld_pt_base + (gpa >> PDE_SHIFT);
	} else {
		pt_page = info->ept.sworld_pgtable_base + TRUSTY_PML4_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) +
			TRUSTY_PDPT_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) + TRUSTY_PD_PAGE_NUM(TRUSTY_EPT_REBASE_GPA) +
			((gpa - TRUSTY_EPT_REBASE_GPA) >> PDE_SHIFT);
	}
	(void)memset(pt_page, 0U, PAGE_SIZE);
	return pt_page;
}

static inline void *ept_get_sworld_memory_base(const union pgtable_pages_info *info)
{
	return info->ept.sworld_memory_base;
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

	if (is_sos_vm(vm)) {
		ept_pages_info[vm_id].ept.top_address_space = EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE);
		ept_pages_info[vm_id].ept.nworld_pml4_base = sos_vm_pml4_pages[0U];
		ept_pages_info[vm_id].ept.nworld_pdpt_base = sos_vm_pdpt_pages[0U];
		ept_pages_info[vm_id].ept.nworld_pd_base = sos_vm_pd_pages[0U];
#ifndef CONFIG_LAST_LEVEL_EPT_AT_BOOT
		ept_pages_info[vm_id].ept.nworld_pt_base = sos_vm_pt_pages[0U];
#endif
	} else if (is_prelaunched_vm(vm)) {
		ept_pages_info[vm_id].ept.top_address_space = PRE_VM_EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE);
		ept_pages_info[vm_id].ept.nworld_pml4_base = pre_uos_nworld_pml4_pages[vm_id];
		ept_pages_info[vm_id].ept.nworld_pdpt_base = pre_uos_nworld_pdpt_pages[vm_id];
		ept_pages_info[vm_id].ept.nworld_pd_base = pre_uos_nworld_pd_pages[vm_id];
#ifndef CONFIG_LAST_LEVEL_EPT_AT_BOOT
		ept_pages_info[vm_id].ept.nworld_pt_base = pre_uos_nworld_pt_pages[vm_id];
#endif
	} else {
		uint16_t sos_vm_id = (get_sos_vm())->vm_id;
		uint16_t page_idx = vmid_2_rel_vmid(sos_vm_id, vm_id) - 1U;

		ept_pages_info[vm_id].ept.top_address_space = EPT_ADDRESS_SPACE(CONFIG_UOS_RAM_SIZE);
		ept_pages_info[vm_id].ept.nworld_pml4_base = post_uos_nworld_pml4_pages[page_idx];
		ept_pages_info[vm_id].ept.nworld_pdpt_base = post_uos_nworld_pdpt_pages[page_idx];
		ept_pages_info[vm_id].ept.nworld_pd_base = post_uos_nworld_pd_pages[page_idx];
#ifndef CONFIG_LAST_LEVEL_EPT_AT_BOOT
		ept_pages_info[vm_id].ept.nworld_pt_base = post_uos_nworld_pt_pages[page_idx];
#endif
		ept_pages_info[vm_id].ept.sworld_pgtable_base = post_uos_sworld_pgtable_pages[page_idx];
		ept_pages_info[vm_id].ept.sworld_memory_base = post_uos_sworld_memory[page_idx];
		mem_ops->get_sworld_memory_base = ept_get_sworld_memory_base;
	}
	mem_ops->info = &ept_pages_info[vm_id];
	mem_ops->get_default_access_right = ept_get_default_access_right;
	mem_ops->pgentry_present = ept_pgentry_present;
	mem_ops->get_pml4_page = ept_get_pml4_page;
	mem_ops->get_pdpt_page = ept_get_pdpt_page;
	mem_ops->get_pd_page = ept_get_pd_page;
	mem_ops->get_pt_page = ept_get_pt_page;
	mem_ops->clflush_pagewalk = ept_clflush_pagewalk;
	mem_ops->large_page_support = large_page_support;

	/* Mitigation for issue "Machine Check Error on Page Size Change" */
	if (is_ept_force_4k_ipage()) {
		mem_ops->tweak_exe_right = ept_tweak_exe_right;
		mem_ops->recover_exe_right = ept_recover_exe_right;
		/* For RTVM, build 4KB page mapping in EPT */
		if (is_rt_vm(vm)) {
			mem_ops->large_page_support = large_page_not_support;
		}
	} else {
		mem_ops->tweak_exe_right = nop_tweak_exe_right;
		mem_ops->recover_exe_right = nop_recover_exe_right;
	}
}
