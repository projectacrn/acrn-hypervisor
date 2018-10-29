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

#include <hypervisor.h>
#include <reloc.h>

static void *mmu_pml4_addr;
static void *sanitized_page[CPU_PAGE_SIZE];

static struct vmx_capability {
	uint32_t ept;
	uint32_t vpid;
} vmx_caps;

/*
 * If the logical processor is in VMX non-root operation and
 * the "enable VPID" VM-execution control is 1, the current VPID
 * is the value of the VPID VM-execution control field in the VMCS.
 * (VM entry ensures that this value is never 0000H).
 */
static uint16_t vmx_vpid_nr = VMX_MIN_NR_VPID;

#define INVEPT_TYPE_SINGLE_CONTEXT      1UL
#define INVEPT_TYPE_ALL_CONTEXTS        2UL
#define VMFAIL_INVALID_EPT_VPID				\
	"       jnc 1f\n"				\
	"       mov $1, %0\n"      /* CF: error = 1 */	\
	"       jmp 3f\n"				\
	"1:     jnz 2f\n"				\
	"       mov $2, %0\n"      /* ZF: error = 2 */	\
	"       jmp 3f\n"				\
	"2:     mov $0, %0\n"				\
	"3:"

struct invept_desc {
	uint64_t eptp;
	uint64_t res;
};

static inline void local_invvpid(uint64_t type, uint16_t vpid, uint64_t gva)
{
	int error = 0;

	struct {
		uint32_t vpid : 16;
		uint32_t rsvd1 : 16;
		uint32_t rsvd2 : 32;
		uint64_t gva;
	} operand = { vpid, 0U, 0U, gva };

	asm volatile ("invvpid %1, %2\n"
			VMFAIL_INVALID_EPT_VPID
			: "=r" (error)
			: "m" (operand), "r" (type)
			: "memory");

	ASSERT(error == 0, "invvpid error");
}

static inline void local_invept(uint64_t type, struct invept_desc desc)
{
	int error = 0;

	asm volatile ("invept %1, %2\n"
			VMFAIL_INVALID_EPT_VPID
			: "=r" (error)
			: "m" (desc), "r" (type)
			: "memory");

	ASSERT(error == 0, "invept error");
}

static inline bool cpu_has_vmx_ept_cap(uint32_t bit_mask)
{
	return ((vmx_caps.ept & bit_mask) != 0U);
}

static inline bool cpu_has_vmx_vpid_cap(uint32_t bit_mask)
{
	return ((vmx_caps.vpid & bit_mask) != 0U);
}

int check_vmx_mmu_cap(void)
{
	uint64_t val;

	/* Read the MSR register of EPT and VPID Capability -  SDM A.10 */
	val = msr_read(MSR_IA32_VMX_EPT_VPID_CAP);
	vmx_caps.ept = (uint32_t) val;
	vmx_caps.vpid = (uint32_t) (val >> 32U);

	if (!cpu_has_vmx_ept_cap(VMX_EPT_INVEPT)) {
		pr_fatal("%s, invept not supported\n", __func__);
		return -ENODEV;
	}

	if (!cpu_has_vmx_vpid_cap(VMX_VPID_INVVPID) ||
		!cpu_has_vmx_vpid_cap(VMX_VPID_INVVPID_SINGLE_CONTEXT) ||
		!cpu_has_vmx_vpid_cap(VMX_VPID_INVVPID_GLOBAL_CONTEXT)) {
		pr_fatal("%s, invvpid not supported\n", __func__);
		return -ENODEV;
	}

	if (!cpu_has_vmx_ept_cap(VMX_EPT_1GB_PAGE)) {
		pr_fatal("%s, ept not support 1GB large page\n", __func__);
		return -ENODEV;
	}

	return 0;
}

uint16_t allocate_vpid(void)
{
	uint16_t vpid = atomic_xadd16(&vmx_vpid_nr, 1U);

	/* TODO: vpid overflow */
	if (vpid >= VMX_MAX_NR_VPID) {
		pr_err("%s, vpid overflow\n", __func__);
		/*
		 * set vmx_vpid_nr to VMX_MAX_NR_VPID to disable vpid
		 * since next atomic_xadd16 will always large than
		 * VMX_MAX_NR_VPID.
		 */
		vmx_vpid_nr = VMX_MAX_NR_VPID;
		vpid = 0U;
	}

	return vpid;
}

void flush_vpid_single(uint16_t vpid)
{
	if (vpid == 0U) {
		return;
	}

	local_invvpid(VMX_VPID_TYPE_SINGLE_CONTEXT, vpid, 0UL);
}

void flush_vpid_global(void)
{
	local_invvpid(VMX_VPID_TYPE_ALL_CONTEXT, 0U, 0UL);
}

void invept(const struct vcpu *vcpu)
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

uint64_t get_paging_pml4(void)
{
	/* Return address to caller */
	return hva2hpa(mmu_pml4_addr);
}

void enable_paging(uint64_t pml4_base_addr)
{
	uint64_t tmp64 = 0UL;

	/* Enable Write Protect, inhibiting writing to read-only pages */
	CPU_CR_READ(cr0, &tmp64);
	CPU_CR_WRITE(cr0, tmp64 | CR0_WP);

	CPU_CR_WRITE(cr3, pml4_base_addr);
}

void enable_smep(void)
{
	uint64_t val64 = 0UL;

	/* Enable CR4.SMEP*/
	CPU_CR_READ(cr4, &val64);
	CPU_CR_WRITE(cr4, val64 | CR4_SMEP);
}


void init_paging(void)
{
	struct e820_entry *entry;
	uint64_t hv_hpa;
	uint32_t i;
	uint64_t low32_max_ram = 0UL;
	uint64_t high64_max_ram;
	uint64_t attr_uc = (PAGE_TABLE | PAGE_CACHE_UC);

	pr_dbg("HV MMU Initialization");

	/* Allocate memory for Hypervisor PML4 table */
	mmu_pml4_addr = ppt_mem_ops.get_pml4_page(ppt_mem_ops.info, 0UL);

	init_e820();
	obtain_e820_mem_info();

	/* align to 2MB */
	high64_max_ram = (e820_mem.mem_top + PDE_SIZE - 1UL) & PDE_MASK;

	if (high64_max_ram > (CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE) ||
			high64_max_ram < (1UL << 32U)) {
		panic("Please configure HV_ADDRESS_SPACE correctly!\n");
	}

	/* Map all memory regions to UC attribute */
	mmu_add((uint64_t *)mmu_pml4_addr, e820_mem.mem_bottom, e820_mem.mem_bottom,
		high64_max_ram - e820_mem.mem_bottom, attr_uc, &ppt_mem_ops);

	/* Modify WB attribute for E820_TYPE_RAM */
	for (i = 0U; i < e820_entries; i++) {
		entry = &e820[i];
		if (entry->type == E820_TYPE_RAM) {
			if (entry->baseaddr < (1UL << 32U)) {
				uint64_t end = entry->baseaddr + entry->length;
				if (end < (1UL << 32U) && (end > low32_max_ram)) {
					low32_max_ram = end;
				}
			}
		}
	}

	mmu_modify_or_del((uint64_t *)mmu_pml4_addr, 0UL, (low32_max_ram + PDE_SIZE - 1UL) & PDE_MASK,
			PAGE_CACHE_WB, PAGE_CACHE_MASK, &ppt_mem_ops, MR_MODIFY);

	mmu_modify_or_del((uint64_t *)mmu_pml4_addr, (1UL << 32U), high64_max_ram - (1UL << 32U),
			PAGE_CACHE_WB, PAGE_CACHE_MASK, &ppt_mem_ops, MR_MODIFY);

	/* set the paging-structure entries' U/S flag
	 * to supervisor-mode for hypervisor owned memroy.
	 */
	hv_hpa = get_hv_image_base();
	mmu_modify_or_del((uint64_t *)mmu_pml4_addr, hv_hpa & PDE_MASK,
		CONFIG_HV_RAM_SIZE + ((hv_hpa & (PDE_SIZE - 1UL)) != 0UL) ? PDE_SIZE : 0UL,
			PAGE_CACHE_WB, PAGE_CACHE_MASK | PAGE_USER,
			&ppt_mem_ops, MR_MODIFY);

	/* Enable paging */
	enable_paging(hva2hpa(mmu_pml4_addr));

	/* set ptep in sanitized_page point to itself */
	sanitize_pte((uint64_t *)sanitized_page);
}

void *alloc_paging_struct(void)
{
	void *ptr = NULL;

	/* Allocate a page from Hypervisor heap */
	ptr = alloc_page();

	ASSERT(ptr != NULL, "page alloc failed!");
	(void)memset(ptr, 0U, CPU_PAGE_SIZE);

	return ptr;
}

void free_paging_struct(void *ptr)
{
	if (ptr != NULL) {
		(void)memset(ptr, 0U, CPU_PAGE_SIZE);
		free(ptr);
	}
}

bool check_continuous_hpa(struct vm *vm, uint64_t gpa_arg, uint64_t size_arg)
{
	uint64_t curr_hpa;
	uint64_t next_hpa;
	uint64_t gpa = gpa_arg;
	uint64_t size = size_arg;

	/* if size <= PAGE_SIZE_4K, it is continuous,no need check
	 * if size > PAGE_SIZE_4K, need to fetch next page
	 */
	while (size > PAGE_SIZE_4K) {
		curr_hpa = gpa2hpa(vm, gpa);
		gpa += PAGE_SIZE_4K;
		next_hpa = gpa2hpa(vm, gpa);
		if ((curr_hpa == INVALID_HPA) || (next_hpa == INVALID_HPA)
			|| (next_hpa != (curr_hpa + PAGE_SIZE_4K))) {
			return false;
		}
		size -= PAGE_SIZE_4K;
	}
	return true;

}
