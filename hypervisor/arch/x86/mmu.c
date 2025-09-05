/*-
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2017-2025 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <types.h>
#include <atomic.h>
#include <asm/cpufeatures.h>
#include <asm/pgtable.h>
#include <asm/cpu_caps.h>
#include <asm/mmu.h>
#include <asm/vmx.h>
#include <reloc.h>
#include <asm/guest/vm.h>
#include <asm/boot/ld_sym.h>
#include <logmsg.h>
#include <misc_cfg.h>

static void *ppt_mmu_pml4_addr;
/**
 * @brief The sanitized page
 *
 * The sanitized page is used to mitigate l1tf.
 */
static uint8_t sanitized_page[PAGE_SIZE] __aligned(PAGE_SIZE);

/* PPT VA and PA are identical mapping */
#define PPT_PML4_PAGE_NUM	PML4_PAGE_NUM(MAX_PHY_ADDRESS_SPACE)
#define PPT_PDPT_PAGE_NUM	PDPT_PAGE_NUM(MAX_PHY_ADDRESS_SPACE)
#define PPT_PT_PAGE_NUM	0UL	/* not support 4K granularity page mapping */

/* Please refer to how the ept page num  was calculated */
uint64_t get_ppt_page_num(void)
{
       uint64_t ppt_pd_page_num = PD_PAGE_NUM(get_e820_ram_size() + MEM_4G) + CONFIG_MAX_PCI_DEV_NUM * 6U;

       /* must be a multiple of 64 */
       return roundup((PPT_PML4_PAGE_NUM + PPT_PDPT_PAGE_NUM + ppt_pd_page_num + PPT_PT_PAGE_NUM), 64U);
}

/* ppt: primary page pool */
static struct page_pool ppt_page_pool;

/* @pre: The PPT and EPT have same page granularity */
static inline bool ppt_large_page_support(enum _page_table_level level, __unused uint64_t prot)
{
	bool support;

	if (level == IA32E_PD) {
		support = true;
	} else if (level == IA32E_PDPT) {
		support = pcpu_has_cap(X86_FEATURE_PAGE1GB);
	} else {
		support = false;
	}

	return support;
}

static inline void ppt_clflush_pagewalk(const void* entry __attribute__((unused)))
{
}


static inline void ppt_nop_tweak_exe_right(uint64_t *entry __attribute__((unused))) {}
static inline void ppt_nop_recover_exe_right(uint64_t *entry __attribute__((unused))) {}

static const struct pgtable ppt_pgtable = {
	.default_access_right = (PAGE_PRESENT | PAGE_RW | PAGE_USER),
	.pgentry_present_mask = PAGE_PRESENT,
	.pool = &ppt_page_pool,
	.large_page_support = ppt_large_page_support,
	.clflush_pagewalk = ppt_clflush_pagewalk,
	.tweak_exe_right = ppt_nop_tweak_exe_right,
	.recover_exe_right = ppt_nop_recover_exe_right,
};

/*
 * @pre: the combined type and vpid is correct
 */
static inline void local_invvpid(uint64_t type, uint16_t vpid, uint64_t gva)
{
	const struct invvpid_operand operand = { vpid, 0U, 0U, gva };

	if (asm_invvpid(operand, type) != 0) {
		pr_dbg("%s, failed. type = %lu, vpid = %u", __func__, type, vpid);
	}
}

/*
 * @pre: the combined type and EPTP is correct
 */
static inline void local_invept(uint64_t type, struct invept_desc desc)
{
	if (asm_invept(type, desc) != 0) {
		pr_dbg("%s, failed. type = %lu, eptp = 0x%lx", __func__, type, desc.eptp);
	}
}

void flush_vpid_single(uint16_t vpid)
{
	if (vpid != 0U) {
		local_invvpid(VMX_VPID_TYPE_SINGLE_CONTEXT, vpid, 0UL);
	}
}

void flush_vpid_global(void)
{
	local_invvpid(VMX_VPID_TYPE_ALL_CONTEXT, 0U, 0UL);
}

void invept(const void *eptp)
{
	struct invept_desc desc = {0};

	if (pcpu_has_vmx_ept_vpid_cap(VMX_EPT_INVEPT_SINGLE_CONTEXT)) {
		desc.eptp = hva2hpa(eptp) | (3UL << 3U) | 6UL;
		local_invept(INVEPT_TYPE_SINGLE_CONTEXT, desc);
	} else if (pcpu_has_vmx_ept_vpid_cap(VMX_EPT_INVEPT_GLOBAL_CONTEXT)) {
		local_invept(INVEPT_TYPE_ALL_CONTEXTS, desc);
	} else {
		/* Neither type of INVEPT is supported. Skip. */
	}
}

void enable_paging(void)
{
	uint64_t tmp64 = 0UL;

	/* Initialize IA32_PAT according to ISDM 11.12.4 Programming the PAT */
	msr_write(MSR_IA32_PAT, PAT_POWER_ON_VALUE);

	/*
	 * Enable MSR IA32_EFER.NXE bit,to prevent
	 * instruction fetching from pages with XD bit set.
	 */
	tmp64 = msr_read(MSR_IA32_EFER);

	/*
	 * SCE bit is not used by the host. However we set this bit so that
	 * it's highly likely that the value of IA32_EFER the host and the guest
	 * is identical, and we don't need to switch this MSR on VMX transitions
	 */
	tmp64 |= MSR_IA32_EFER_NXE_BIT | MSR_IA32_EFER_SCE_BIT;
	msr_write(MSR_IA32_EFER, tmp64);

	/* Enable Write Protect, inhibiting writing to read-only pages */
	CPU_CR_READ(cr0, &tmp64);
	CPU_CR_WRITE(cr0, tmp64 | CR0_WP);

	/* HPA->HVA is 1:1 mapping at this moment, simply treat ppt_mmu_pml4_addr as HPA. */
	CPU_CR_WRITE(cr3, ppt_mmu_pml4_addr);
}

void enable_smep(void)
{
	uint64_t val64 = 0UL;

	/* Enable CR4.SMEP*/
	CPU_CR_READ(cr4, &val64);
	CPU_CR_WRITE(cr4, val64 | CR4_SMEP);
}

void enable_smap(void)
{
	uint64_t val64 = 0UL;

	/* Enable CR4.SMAP*/
	CPU_CR_READ(cr4, &val64);
	CPU_CR_WRITE(cr4, val64 | CR4_SMAP);
}

/*
 * Clean USER bit in page table to update memory pages to be owned by hypervisor.
 */
void set_paging_supervisor(uint64_t base, uint64_t size)
{
	uint64_t base_aligned;
	uint64_t size_aligned;
	uint64_t region_end = base + size;

	/*rounddown base to 2MBytes aligned.*/
	base_aligned = round_pde_down(base);
	size_aligned = region_end - base_aligned;

	pgtable_modify_or_del_map((uint64_t *)ppt_mmu_pml4_addr, base_aligned,
		round_pde_up(size_aligned), 0UL, PAGE_USER, &ppt_pgtable, MR_MODIFY);
}

void set_paging_nx(uint64_t base, uint64_t size)
{
	uint64_t region_end = base + size;
	uint64_t base_aligned = round_pde_down(base);
	uint64_t size_aligned = round_pde_up(region_end - base_aligned);

	pgtable_modify_or_del_map((uint64_t *)ppt_mmu_pml4_addr,
		base_aligned, size_aligned, PAGE_NX, 0UL, &ppt_pgtable, MR_MODIFY);
}

void set_paging_x(uint64_t base, uint64_t size)
{
	uint64_t region_end = base + size;
	uint64_t base_aligned = round_pde_down(base);
	uint64_t size_aligned = round_pde_up(region_end - base_aligned);

	pgtable_modify_or_del_map((uint64_t *)ppt_mmu_pml4_addr,
		base_aligned, size_aligned, 0UL, PAGE_NX, &ppt_pgtable, MR_MODIFY);
}

void allocate_ppt_pages(void)
{
	uint64_t page_base;
	uint64_t bitmap_size = get_ppt_page_num() / 8;

	page_base = e820_alloc_memory(sizeof(struct page) * get_ppt_page_num(), MEM_4G);
	ppt_page_pool.bitmap = (uint64_t *)e820_alloc_memory(bitmap_size, MEM_4G);

	ppt_page_pool.start_page = (struct page *)(void *)page_base;
	ppt_page_pool.bitmap_size = bitmap_size / sizeof(uint64_t);
	ppt_page_pool.dummy_page = NULL;

	memset(ppt_page_pool.bitmap, 0, bitmap_size);
}

void init_paging(void)
{
	uint64_t hv_hva;
	uint32_t i;
	uint64_t low32_max_ram = 0UL;
	uint64_t high64_min_ram = ~0UL;
	uint64_t high64_max_ram = MEM_4G;

	struct acrn_boot_info *abi = get_acrn_boot_info();
	const struct abi_mmap *entry;
	uint32_t entries_count = abi->mmap_entries;
	const struct abi_mmap *p_mmap = abi->mmap_entry;

	pr_dbg("HV MMU Initialization");

	init_sanitized_page((uint64_t *)sanitized_page, hva2hpa_early(sanitized_page));

	/* Allocate memory for Hypervisor PML4 table */
	ppt_mmu_pml4_addr = pgtable_create_root(&ppt_pgtable);

	/* Modify WB attribute for E820_TYPE_RAM */
	for (i = 0U; i < entries_count; i++) {
		entry = p_mmap + i;
		if (entry->type == MMAP_TYPE_RAM) {
			uint64_t end = entry->baseaddr + entry->length;
			if (end < MEM_4G) {
				low32_max_ram = max(end, low32_max_ram);
			} else {
				high64_min_ram = min(entry->baseaddr, high64_min_ram);
				high64_max_ram = max(end, high64_max_ram);
			}
		}
	}

	low32_max_ram = round_pde_up(low32_max_ram);
	high64_max_ram = round_pde_down(high64_max_ram);

	/* Map [0, low32_max_ram) and [high64_min_ram, high64_max_ram) RAM regions as WB attribute */
	pgtable_add_map((uint64_t *)ppt_mmu_pml4_addr, 0UL, 0UL,
			low32_max_ram, PAGE_ATTR_USER | PAGE_CACHE_WB, &ppt_pgtable);

	if (high64_max_ram > high64_min_ram) {
		pgtable_add_map((uint64_t *)ppt_mmu_pml4_addr, high64_min_ram, high64_min_ram,
				high64_max_ram - high64_min_ram, PAGE_ATTR_USER | PAGE_CACHE_WB, &ppt_pgtable);
	}
	/* Map [low32_max_ram, 4G) and [MMIO64_START, MMIO64_END) MMIO regions as UC attribute */
	pgtable_add_map((uint64_t *)ppt_mmu_pml4_addr, low32_max_ram, low32_max_ram,
		MEM_4G - low32_max_ram, PAGE_ATTR_USER | PAGE_CACHE_UC, &ppt_pgtable);
	if ((MMIO64_START != ~0UL) && (MMIO64_END != 0UL)) {
		pgtable_add_map((uint64_t *)ppt_mmu_pml4_addr, MMIO64_START, MMIO64_START,
			(MMIO64_END - MMIO64_START), PAGE_ATTR_USER | PAGE_CACHE_UC, &ppt_pgtable);
	}

	/*
	 * set the paging-structure entries' U/S flag to supervisor-mode for hypervisor owned memory.
	 * (exclude the memory reserve for trusty)
	 *
	 * Before the new PML4 take effect in enable_paging(), HPA->HVA is always 1:1 mapping,
	 * simply treat the return value of get_hv_image_base() as HPA.
	 */
	hv_hva = get_hv_image_base();
	pgtable_modify_or_del_map((uint64_t *)ppt_mmu_pml4_addr, hv_hva & PDE_MASK,
			get_hv_image_size() + (((hv_hva & (PDE_SIZE - 1UL)) != 0UL) ? PDE_SIZE : 0UL),
			PAGE_CACHE_WB, PAGE_CACHE_MASK | PAGE_USER, &ppt_pgtable, MR_MODIFY);

	/*
	 * remove 'NX' bit for pages that contain hv code section, as by default XD bit is set for
	 * all pages, including pages for guests.
	 */
	pgtable_modify_or_del_map((uint64_t *)ppt_mmu_pml4_addr, round_pde_down(hv_hva),
			round_pde_up((uint64_t)&ld_text_end) - round_pde_down(hv_hva), 0UL,
			PAGE_NX, &ppt_pgtable, MR_MODIFY);
#if ((SERVICE_VM_NUM == 1) && (MAX_TRUSTY_VM_NUM > 0))
	pgtable_modify_or_del_map((uint64_t *)ppt_mmu_pml4_addr, (uint64_t)get_sworld_memory_base(),
			TRUSTY_RAM_SIZE * MAX_TRUSTY_VM_NUM, PAGE_USER, 0UL, &ppt_pgtable, MR_MODIFY);
#endif

	/* Enable paging */
	enable_paging();
}

void flush_tlb(uint64_t addr)
{
	invlpg(addr);
}

void flush_tlb_range(uint64_t addr, uint64_t size)
{
	uint64_t linear_addr;

	for (linear_addr = addr; linear_addr < (addr + size); linear_addr += PAGE_SIZE) {
		invlpg(linear_addr);
	}
}

void flush_invalidate_all_cache(void)
{
	wbinvd();
}

void flush_cacheline(const volatile void *p)
{
	clflush(p);
}

void flush_cache_range(const volatile void *p, uint64_t size)
{
	uint64_t i;

	for (i = 0UL; i < size; i += CACHE_LINE_SIZE) {
		clflushopt(p + i);
	}
}
