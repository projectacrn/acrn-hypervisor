/*-
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
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
#include <page.h>
#include <pgtable.h>
#include <cpu_caps.h>
#include <mmu.h>
#include <vmx.h>
#include <reloc.h>
#include <vcpu.h>
#include <vm.h>
#include <ld_sym.h>
#include <logmsg.h>

static void *ppt_mmu_pml4_addr;
static uint8_t sanitized_page[PAGE_SIZE] __aligned(PAGE_SIZE);

#define INVEPT_TYPE_SINGLE_CONTEXT      1UL
#define INVEPT_TYPE_ALL_CONTEXTS        2UL
#define VMFAIL_INVALID_EPT_VPID				\
	"       jnc 1f\n"				\
	"       mov $1, %0\n"    /* CF: error = 1 */	\
	"       jmp 3f\n"				\
	"1:     jnz 2f\n"				\
	"       mov $2, %0\n"    /* ZF: error = 2 */	\
	"       jmp 3f\n"				\
	"2:     mov $0, %0\n"				\
	"3:"

struct invvpid_operand {
	uint32_t vpid : 16;
	uint32_t rsvd1 : 16;
	uint32_t rsvd2 : 32;
	uint64_t gva;
};

struct invept_desc {
	uint64_t eptp;
	uint64_t res;
};

static inline int32_t asm_invvpid(const struct invvpid_operand operand, uint64_t type)
{
	int32_t error;
	asm volatile ("invvpid %1, %2\n"
			VMFAIL_INVALID_EPT_VPID
			: "=r" (error)
			: "m" (operand), "r" (type)
			: "memory");
	return error;
}

/*
 * @pre: the combined type and vpid is correct
 */
static inline void local_invvpid(uint64_t type, uint16_t vpid, uint64_t gva)
{
	const struct invvpid_operand operand = { vpid, 0U, 0U, gva };

	if (asm_invvpid(operand, type) != 0) {
		pr_dbg("%s, failed. type = %llu, vpid = %u", __func__, type, vpid);
	}
}

static inline int32_t asm_invept(uint64_t type, struct invept_desc desc)
{
	int32_t error;
	asm volatile ("invept %1, %2\n"
			VMFAIL_INVALID_EPT_VPID
			: "=r" (error)
			: "m" (desc), "r" (type)
			: "memory");
	return error;
}

/*
 * @pre: the combined type and EPTP is correct
 */
static inline void local_invept(uint64_t type, struct invept_desc desc)
{
	if (asm_invept(type, desc) != 0) {
		pr_dbg("%s, failed. type = %llu, eptp = 0x%llx", __func__, type, desc.eptp);
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

void invept(const struct acrn_vcpu *vcpu)
{
	struct invept_desc desc = {0};

	if (cpu_has_vmx_ept_cap(VMX_EPT_INVEPT_SINGLE_CONTEXT)) {
		desc.eptp = hva2hpa(vcpu->vm->arch_vm.nworld_eptp) |
				(3UL << 3U) | 6UL;
		local_invept(INVEPT_TYPE_SINGLE_CONTEXT, desc);
		if (vcpu->vm->sworld_control.flag.active != 0UL) {
			desc.eptp = hva2hpa(vcpu->vm->arch_vm.sworld_eptp)
				| (3UL << 3U) | 6UL;
			local_invept(INVEPT_TYPE_SINGLE_CONTEXT, desc);
		}
	} else if (cpu_has_vmx_ept_cap(VMX_EPT_INVEPT_GLOBAL_CONTEXT)) {
		local_invept(INVEPT_TYPE_ALL_CONTEXTS, desc);
	} else {
		/* Neither type of INVEPT is supported. Skip. */
	}
}

static inline uint64_t get_sanitized_page(void)
{
	return hva2hpa(sanitized_page);
}

void sanitize_pte_entry(uint64_t *ptep)
{
	set_pgentry(ptep, get_sanitized_page());
}

void sanitize_pte(uint64_t *pt_page)
{
	uint64_t i;
	for (i = 0UL; i < PTRS_PER_PTE; i++) {
		sanitize_pte_entry(pt_page + i);
	}
}

void enable_paging(void)
{
	uint64_t tmp64 = 0UL;

	/*
	 * Enable MSR IA32_EFER.NXE bit,to prevent
	 * instruction fetching from pages with XD bit set.
	 */
	tmp64 = msr_read(MSR_IA32_EFER);
	tmp64 |= MSR_IA32_EFER_NXE_BIT;
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
 * Update memory pages to be owned by hypervisor.
 */
void hv_access_memory_region_update(uint64_t base, uint64_t size)
{
	uint64_t base_aligned;
	uint64_t size_aligned;
	uint64_t region_end = base + size;

	/*rounddown base to 2MBytes aligned.*/
	base_aligned = round_pde_down(base);
	size_aligned = region_end - base_aligned;

	mmu_modify_or_del((uint64_t *)ppt_mmu_pml4_addr, base_aligned,
		round_pde_up(size_aligned), 0UL, PAGE_USER, &ppt_mem_ops, MR_MODIFY);
}

void init_paging(void)
{
	uint64_t hv_hpa, text_end, size;
	uint32_t i;
	uint64_t low32_max_ram = 0UL;
	uint64_t high64_max_ram;
	uint64_t attr_uc = (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_CACHE_UC | PAGE_NX);

	const struct e820_entry *entry;
	uint32_t entries_count = get_e820_entries_count();
	const struct e820_entry *p_e820 = get_e820_entry();
	const struct e820_mem_params *p_e820_mem_info = get_e820_mem_info();

	pr_dbg("HV MMU Initialization");

	/* align to 2MB */
	high64_max_ram = round_pde_up(p_e820_mem_info->mem_top);
	if ((high64_max_ram > (CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE)) ||
			(high64_max_ram < (1UL << 32U))) {
		panic("Please configure HV_ADDRESS_SPACE correctly!\n");
	}

	/* Allocate memory for Hypervisor PML4 table */
	ppt_mmu_pml4_addr = ppt_mem_ops.get_pml4_page(ppt_mem_ops.info);

	/* Map all memory regions to UC attribute */
	mmu_add((uint64_t *)ppt_mmu_pml4_addr, 0UL, 0UL, high64_max_ram - 0UL, attr_uc, &ppt_mem_ops);

	/* Modify WB attribute for E820_TYPE_RAM */
	for (i = 0U; i < entries_count; i++) {
		entry = p_e820 + i;
		if (entry->type == E820_TYPE_RAM) {
			if (entry->baseaddr < (1UL << 32U)) {
				uint64_t end = entry->baseaddr + entry->length;
				if ((end < (1UL << 32U)) && (end > low32_max_ram)) {
					low32_max_ram = end;
				}
			}
		}
	}

	mmu_modify_or_del((uint64_t *)ppt_mmu_pml4_addr, 0UL, round_pde_up(low32_max_ram),
			PAGE_CACHE_WB, PAGE_CACHE_MASK, &ppt_mem_ops, MR_MODIFY);

	mmu_modify_or_del((uint64_t *)ppt_mmu_pml4_addr, (1UL << 32U), high64_max_ram - (1UL << 32U),
			PAGE_CACHE_WB, PAGE_CACHE_MASK, &ppt_mem_ops, MR_MODIFY);

	/*
	 * set the paging-structure entries' U/S flag to supervisor-mode for hypervisor owned memroy.
	 * (exclude the memory reserve for trusty)
	 *
	 * Before the new PML4 take effect in enable_paging(), HPA->HVA is always 1:1 mapping,
	 * simply treat the return value of get_hv_image_base() as HPA.
	 */
	hv_hpa = get_hv_image_base();
	mmu_modify_or_del((uint64_t *)ppt_mmu_pml4_addr, hv_hpa & PDE_MASK,
			CONFIG_HV_RAM_SIZE + (((hv_hpa & (PDE_SIZE - 1UL)) != 0UL) ? PDE_SIZE : 0UL),
			PAGE_CACHE_WB, PAGE_CACHE_MASK | PAGE_USER, &ppt_mem_ops, MR_MODIFY);

	size = ((uint64_t)&ld_text_end - hv_hpa);
	text_end = hv_hpa + size;
	/*
	 * remove 'NX' bit for pages that contain hv code section, as by default XD bit is set for
	 * all pages, including pages for guests.
	 */
	mmu_modify_or_del((uint64_t *)ppt_mmu_pml4_addr, round_pde_down(hv_hpa),
			round_pde_up(text_end) - round_pde_down(hv_hpa), 0UL,
			PAGE_NX, &ppt_mem_ops, MR_MODIFY);

	mmu_modify_or_del((uint64_t *)ppt_mmu_pml4_addr, (uint64_t)get_reserve_sworld_memory_base(),
			TRUSTY_RAM_SIZE * (CONFIG_MAX_VM_NUM - 1U), PAGE_USER, 0UL, &ppt_mem_ops, MR_MODIFY);

#ifdef CONFIG_DMAR_PARSE_ENABLED
	for (i = 0U; i < entries_count; i++) {
		entry = p_e820 + i;
		if ((entry->type == E820_TYPE_ACPI_RECLAIM) || (entry->type == E820_TYPE_RESERVED)) {
			hv_access_memory_region_update(entry->baseaddr, entry->length);
		}
	}
#endif
	/* Enable paging */
	enable_paging();

	/* set ptep in sanitized_page point to itself */
	sanitize_pte((uint64_t *)sanitized_page);
}
