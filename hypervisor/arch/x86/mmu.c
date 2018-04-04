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
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <bsp_extern.h>
#include <hv_debug.h>

static void *mmu_pml4_addr;

enum mem_map_request_type {
	PAGING_REQUEST_TYPE_MAP = 0,	/* Creates a new mapping. */
	PAGING_REQUEST_TYPE_UNMAP = 1,	/* Removes a pre-existing entry */
	PAGING_REQUEST_TYPE_MODIFY = 2,
	/* Modifies a pre-existing entries attributes. */
	PAGING_REQUEST_TYPE_UNKNOWN,
};

struct mm_capability {
	/* EPT and MMU 1-GByte page supported flag */
	bool ept_1gb_page_supported;
	bool invept_supported;
	bool invept_single_context_supported;
	bool invept_global_context_supported;
	bool invvpid_supported;
	bool invvpid_single_context_supported;
	bool invvpid_global_context_supported;
	bool mmu_1gb_page_supported;
};
static struct mm_capability mm_caps;

#define INVEPT_TYPE_SINGLE_CONTEXT      1UL
#define INVEPT_TYPE_ALL_CONTEXTS        2UL
#define INVEPT_SET_ERROR_CODE				\
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
	uint64_t _res;
};

static inline void _invept(uint64_t type, struct invept_desc desc)
{
	int error = 0;

	asm volatile ("invept %1, %2\n"
			INVEPT_SET_ERROR_CODE
			: "=r" (error)
			: "m" (desc), "r" (type)
			: "memory");

	ASSERT(error == 0, "invept error");
}

static void check_mmu_capability(void)
{
	uint64_t val;
	uint32_t  eax, ebx, ecx, edx;

	memset(&mm_caps, 0, sizeof(struct mm_capability));

	/* Read the MSR register of EPT and VPID Capability -  SDM A.10 */
	val = msr_read(MSR_IA32_VMX_EPT_VPID_CAP);
	mm_caps.ept_1gb_page_supported = (val & MSR_VMX_EPT_VPID_CAP_1GB)
		? (true) : (false);
	mm_caps.invept_supported =
		(val & MSR_VMX_INVEPT) ? (true) : (false);
	mm_caps.invept_single_context_supported =
		(val & MSR_VMX_INVEPT_SINGLE_CONTEXT) ? (true) : (false);
	mm_caps.invept_global_context_supported =
		(val & MSR_VMX_INVEPT_GLOBAL_CONTEXT) ? (true) : (false);
	mm_caps.invvpid_supported =
		(val & MSR_VMX_INVVPID) ? (true) : (false);
	mm_caps.invvpid_single_context_supported =
		(val & MSR_VMX_INVVPID_SINGLE_CONTEXT) ? (true) : (false);
	mm_caps.invvpid_global_context_supported =
		(val & MSR_VMX_INVVPID_GLOBAL_CONTEXT) ? (true) : (false);

	/* Read CPUID to check if PAGE1GB is supported
	 * SDM 4.1.4 If CPUID.80000001H:EDX.Page1GB[bit26]=1,
	 * 1-GByte pages are supported with 4-level paging
	 */
	cpuid(CPUID_EXTEND_FUNCTION_1, &eax, &ebx, &ecx, &edx);
	mm_caps.mmu_1gb_page_supported = (edx & CPUID_EDX_PAGE1GB) ?
							(true) : (false);
}

static inline bool check_invept_single_support(void)
{
	return mm_caps.invept_supported &&
			mm_caps.invept_single_context_supported;
}

static inline bool check_invept_global_support(void)
{
	return mm_caps.invept_supported &&
			mm_caps.invept_global_context_supported;
}

void mmu_invept(struct vcpu *vcpu)
{
	struct invept_desc desc = {0};

	if (check_invept_single_support()) {
		desc.eptp = (uint64_t) vcpu->vm->arch_vm.nworld_eptp
			| (3 << 3) | 6;
		_invept(INVEPT_TYPE_SINGLE_CONTEXT, desc);
		if (vcpu->vm->sworld_control.sworld_enabled) {
			desc.eptp = (uint64_t) vcpu->vm->arch_vm.sworld_eptp
				| (3 << 3) | 6;
			_invept(INVEPT_TYPE_SINGLE_CONTEXT, desc);

		}
	} else if (check_invept_global_support())
		_invept(INVEPT_TYPE_ALL_CONTEXTS, desc);
}

static bool check_mmu_1gb_support(struct map_params *map_params)
{
	bool status = false;

	if (map_params->page_table_type == PTT_EPT)
		status = mm_caps.ept_1gb_page_supported;
	else
		status = mm_caps.mmu_1gb_page_supported;
	return status;
}

static uint32_t map_mem_region(void *vaddr, void *paddr,
		void *table_base, uint64_t attr, uint32_t table_level,
		int table_type, enum mem_map_request_type request_type)
{
	uint64_t table_entry;
	uint32_t table_offset;
	uint32_t mapped_size;

	if (table_base == NULL || table_level >= IA32E_UNKNOWN
	    || request_type >= PAGING_REQUEST_TYPE_UNKNOWN) {
		/* Shouldn't go here */
		ASSERT(false, "Incorrect Arguments. Failed to map region");
	}

	/* switch based on  of table */
	switch (table_level) {
	case IA32E_PDPT:

		/* Get offset to the entry in the PDPT for this address */
		table_offset = IA32E_PDPTE_INDEX_CALC(vaddr);

		/* PS bit must be set for these entries to be mapped */
		attr |= IA32E_PDPTE_PS_BIT;

		/* Set mapped size to 1 GB */
		mapped_size = MEM_1G;

		break;

	case IA32E_PD:

		/* Get offset to the entry in the PD for this address */
		table_offset = IA32E_PDE_INDEX_CALC(vaddr);

		/* PS bit must be set for these entries to be mapped */
		attr |= IA32E_PDE_PS_BIT;

		/* Set mapped size to 2 MB */
		mapped_size = MEM_2M;

		break;

	case IA32E_PT:

		/* Get offset to the entry in the PT for this address */
		table_offset = IA32E_PTE_INDEX_CALC(vaddr);

		/* NOTE: No PS bit in page table entries */

		/* Set mapped size to 4 KB */
		mapped_size = MEM_4K;

		/* If not a EPT entry, see if the PAT bit is set for PDPT entry
		 */
		if ((table_type == PTT_HOST) && (attr & IA32E_PDPTE_PAT_BIT)) {
			/* The PAT bit is set; Clear it and set the page table
			 * PAT bit instead
			 */
			attr &= (uint64_t) (~((uint64_t) IA32E_PDPTE_PAT_BIT));
			attr |= IA32E_PTE_PAT_BIT;
		}

		break;

	case IA32E_PML4:
	default:

		/* Set mapping size to 0 - can't map memory in PML4 */
		mapped_size = 0;

		break;
	}

	/* Check to see if mapping should occur */
	if (mapped_size != 0) {
		switch (request_type) {
		case PAGING_REQUEST_TYPE_MAP:
		{
			/* No need to confirm current table entry
			 * isn't already present
			 * support map-->remap
			 */
			table_entry = ((table_type == PTT_EPT)
					? attr
					: (attr | IA32E_COMM_P_BIT));

			table_entry |= (uint64_t)paddr;

			/* Write the table entry to map this memory */
			MEM_WRITE64(table_base + table_offset, table_entry);
			break;
		}
		case PAGING_REQUEST_TYPE_UNMAP:
		{
			/* Get current table entry */
			uint64_t entry = MEM_READ64(table_base + table_offset);

			if (entry) {
				/* Table is present.
				 * Write the table entry to map this memory
				 */
				MEM_WRITE64(table_base + table_offset, 0);
			}
			break;
		}
		case PAGING_REQUEST_TYPE_MODIFY:
		{
			/* Allow mapping or modification as requested. */
			table_entry = ((table_type == PTT_EPT)
				       ? attr : (attr | IA32E_COMM_P_BIT));

			table_entry |= (uint64_t) paddr;

			/* Write the table entry to map this memory */
			MEM_WRITE64(table_base + table_offset, table_entry);

			break;
		}
		default:
			ASSERT("Bad memory map request type" == 0, "");
			break;
		}
	}

	/* Return mapped size to caller */
	return mapped_size;
}

static uint32_t fetch_page_table_offset(void *addr, uint32_t table_level)
{
	uint32_t table_offset;

	/* Switch based on level of table */
	switch (table_level) {
	case IA32E_PML4:

		/* Get offset to the entry in the PML4
		 * for this address
		 */
		table_offset = IA32E_PML4E_INDEX_CALC(addr);
		break;

	case IA32E_PDPT:

		/* Get offset to the entry in the PDPT
		 * for this address
		 */
		table_offset = IA32E_PDPTE_INDEX_CALC(addr);
		break;

	case IA32E_PD:

		/* Get offset to the entry in the PD
		 * for this address
		 */
		table_offset = IA32E_PDE_INDEX_CALC(addr);
		break;

	case IA32E_PT:
		table_offset = IA32E_PTE_INDEX_CALC(addr);
		break;

	default:
		pr_err("Wrong page table level = 0x%lx", table_level);
		ASSERT(false, "Wrong page table level");
		break;
	}

	return table_offset;
}

static inline uint32_t check_page_table_present(struct map_params *map_params,
		uint64_t table_entry)
{
	if (map_params->page_table_type == PTT_EPT) {
		table_entry &= (IA32E_EPT_R_BIT | IA32E_EPT_W_BIT |
				IA32E_EPT_X_BIT);
	} else {
		table_entry &= (IA32E_COMM_P_BIT);
	}

	return (table_entry) ? PT_PRESENT : PT_NOT_PRESENT;
}

static uint64_t get_table_entry(struct map_params *map_params, void *addr,
		void *table_base, uint32_t table_level)
{
	uint32_t table_offset;
	uint64_t table_entry;
	int status = 0;

	if (table_base == NULL
			|| table_level >= IA32E_UNKNOWN
			|| map_params == NULL)	{
		status = -EINVAL;
	}
	ASSERT(status == 0, "Incorrect Arguments");

	table_offset = fetch_page_table_offset(addr, table_level);

	/* Read the table entry */
	table_entry = MEM_READ64(table_base + table_offset);

	/* Return the next table in the walk */
	return table_entry;
}

static void  *walk_paging_struct(void *addr, void *table_base,
		uint32_t table_level, struct map_params *map_params)
{
	uint32_t table_offset;
	uint64_t table_entry;
	uint64_t table_present;
	/* if  table_level == IA32E_PT Just return the same address
	 * can't walk down any further
	 */
	void *sub_table_addr = ((table_level == IA32E_PT) ? table_base:NULL);
	int status = 0;

	if (table_base == NULL || table_level >= IA32E_UNKNOWN
	    || map_params == NULL) {
		status = -EINVAL;
	}
	ASSERT(status == 0, "Incorrect Arguments");

	table_offset = fetch_page_table_offset(addr, table_level);

	/* See if we can skip the rest */
	if (sub_table_addr != table_base) {
		/* Read the table entry */
		table_entry = MEM_READ64(table_base + table_offset);

		/* Check if EPT entry being created */
		if (map_params->page_table_type == PTT_EPT) {
			/* Set table present bits to any of the
			 * read/write/execute bits
			 */
			table_present = (IA32E_EPT_R_BIT | IA32E_EPT_W_BIT |
					 IA32E_EPT_X_BIT);
		} else {
			/* Set table preset bits to P bit or r/w bit */
			table_present = (IA32E_COMM_P_BIT | IA32E_COMM_RW_BIT);
		}

		/* Determine if a valid entry exists */
		if ((table_entry & table_present) == 0) {
			/* No entry present - need to allocate a new table */
			sub_table_addr =
			    alloc_paging_struct();
			/* Check to ensure memory available for this structure*/
			if (sub_table_addr == 0) {
				/* Error: Unable to find table memory necessary
				 * to map memory
				 */
				ASSERT(sub_table_addr == 0,
					"Fail to find table memory "
					"for map memory");

				return sub_table_addr;
			}

			/* Write entry to current table to reference the new
			 * sub-table
			 */
			MEM_WRITE64(table_base + table_offset,
				    (uint64_t) sub_table_addr | table_present);
		} else {
			/* Get address of the sub-table */
			sub_table_addr = (void *)(table_entry & IA32E_REF_MASK);
		}
	}

	/* Return the next table in the walk */
	return sub_table_addr;
}

void *get_paging_pml4(void)
{
	/* Return address to caller */
	return mmu_pml4_addr;
}

void enable_paging(void *pml4_base_addr)
{
	CPU_CR_WRITE(cr3, (unsigned long)pml4_base_addr);
}

void init_paging(void)
{
	struct map_params map_params;
	struct e820_entry *entry;
	uint32_t i;
	int attr_wb = (MMU_MEM_ATTR_READ |
			MMU_MEM_ATTR_WRITE   |
			MMU_MEM_ATTR_EXECUTE |
			MMU_MEM_ATTR_WB_CACHE);
	int attr_uc = (MMU_MEM_ATTR_READ |
			MMU_MEM_ATTR_WRITE   |
			MMU_MEM_ATTR_EXECUTE |
			MMU_MEM_ATTR_UNCACHED);

	pr_dbg("HV MMU Initialization");

	check_mmu_capability();

	/* Allocate memory for Hypervisor PML4 table */
	mmu_pml4_addr = alloc_paging_struct();

	init_e820();
	obtain_e820_mem_info();

	/* Loop through all memory regions in the e820 table */
	map_params.page_table_type = PTT_HOST;
	map_params.pml4_base = mmu_pml4_addr;

	/* Map all memory regions to UC attribute */
	map_mem(&map_params, (void *)e820_mem.mem_bottom,
			(void *)e820_mem.mem_bottom,
			(e820_mem.mem_top - e820_mem.mem_bottom),
			attr_uc);

	/* Modify WB attribute for E820_TYPE_RAM */
	for (i = 0, entry = &e820[0];
			i < e820_entries;
			i++, entry = &e820[i]) {
		if (entry->type == E820_TYPE_RAM) {
			modify_mem(&map_params, (void *)entry->baseaddr,
					(void *)entry->baseaddr,
					entry->length, attr_wb);
		}
	}

	pr_dbg("Enabling MMU ");

	/* Enable paging */
	enable_paging(mmu_pml4_addr);
}

void *alloc_paging_struct(void)
{
	void *ptr = NULL;

	/* Allocate a page from Hypervisor heap */
	ptr = alloc_page();

	ASSERT(ptr, "page alloc failed!");
	memset(ptr, 0, CPU_PAGE_SIZE);

	return ptr;
}

void free_paging_struct(void *ptr)
{
	if (ptr) {
		memset(ptr, 0, CPU_PAGE_SIZE);
		free(ptr);
	}
}

bool check_continuous_hpa(struct vm *vm, uint64_t gpa, uint64_t size)
{
	uint64_t curr_hpa = 0;
	uint64_t next_hpa = 0;

	/* if size <= PAGE_SIZE_4K, it is continuous,no need check
	 * if size > PAGE_SIZE_4K, need to fetch next page
	 */
	while (size > PAGE_SIZE_4K) {
		curr_hpa = gpa2hpa(vm, gpa);
		gpa += PAGE_SIZE_4K;
		next_hpa = gpa2hpa(vm, gpa);
		if (next_hpa != (curr_hpa + PAGE_SIZE_4K))
			return false;
		size -= PAGE_SIZE_4K;
	}
	return true;

}
uint64_t config_page_table_attr(struct map_params *map_params, uint32_t flags)
{
	int  table_type = map_params->page_table_type;
	uint64_t attr = 0;

	/* Convert generic memory flags to architecture specific attributes */
	/* Check if read access */
	if (flags & MMU_MEM_ATTR_READ) {
		/* Configure for read access */
		attr |= ((table_type == PTT_EPT)
			? IA32E_EPT_R_BIT : MMU_MEM_ATTR_BIT_READ_WRITE);
	}

	/* Check for write access */
	if (flags & MMU_MEM_ATTR_WRITE)	{
		/* Configure for write access */
		attr |= ((table_type == PTT_EPT)
			? IA32E_EPT_W_BIT : MMU_MEM_ATTR_BIT_READ_WRITE);
	}

	/* Check for execute access */
	if (flags & MMU_MEM_ATTR_EXECUTE) {
		/* Configure for execute (EPT only) */
		attr |= ((table_type == PTT_EPT)
			? IA32E_EPT_X_BIT : 0);
	}

	/* EPT & VT-d share the same page tables, set SNP bit
	 * to force snooping of PCIe devices if the page
	 * is cachable
	 */
	if ((flags & MMU_MEM_ATTR_UNCACHED) != MMU_MEM_ATTR_UNCACHED
			&& table_type == PTT_EPT) {
		attr |= IA32E_EPT_SNOOP_CTRL;
	}

	/* Check for cache / memory types */
	if (flags & MMU_MEM_ATTR_WB_CACHE) {
		/* Configure for write back cache */
		attr |= ((table_type == PTT_EPT)
			? IA32E_EPT_WB : MMU_MEM_ATTR_TYPE_CACHED_WB);
	} else if (flags & MMU_MEM_ATTR_WT_CACHE)	{
		/* Configure for write through cache */
		attr |= ((table_type == PTT_EPT)
			? IA32E_EPT_WT : MMU_MEM_ATTR_TYPE_CACHED_WT);
	} else if (flags & MMU_MEM_ATTR_UNCACHED)	{
		/* Configure for uncached */
		attr |= ((table_type == PTT_EPT)
			? IA32E_EPT_UNCACHED : MMU_MEM_ATTR_TYPE_UNCACHED);
	} else if (flags & MMU_MEM_ATTR_WC) {
		/* Configure for write combining */
		attr |= ((table_type == PTT_EPT)
			? IA32E_EPT_WC : MMU_MEM_ATTR_TYPE_WRITE_COMBINED);
	} else {
		/* Configure for write protected */
		attr |= ((table_type == PTT_EPT)
			? IA32E_EPT_WP : MMU_MEM_ATTR_TYPE_WRITE_PROTECTED);
	}
	return attr;

}

void obtain_last_page_table_entry(struct map_params *map_params,
		struct entry_params *entry, void *addr, bool direct)
{
	uint64_t table_entry;
	uint32_t table_present = 0;
	/* Obtain the PML4 address */
	void *table_addr = direct ? (map_params->pml4_base)
				: (map_params->pml4_inverted);

	/* Obtain page table entry from PML4 table*/
	table_entry = get_table_entry(map_params, addr,
			table_addr, IA32E_PML4);
	table_present = check_page_table_present(map_params, table_entry);
	if (table_present == PT_NOT_PRESENT) {
		/* PML4E not present, return PML4 base address */
		entry->entry_level  = IA32E_PML4;
		entry->entry_base = (uint64_t)table_addr;
		entry->entry_present = PT_NOT_PRESENT;
		entry->page_size = check_mmu_1gb_support(map_params) ?
			(PAGE_SIZE_1G) : (PAGE_SIZE_2M);
		entry->entry_off = fetch_page_table_offset(addr, IA32E_PML4);
		entry->entry_val =  table_entry;
		return;
	}

	/* Obtain page table entry from PDPT table*/
	table_addr = (void *)(table_entry & IA32E_REF_MASK);
	table_entry = get_table_entry(map_params, addr,
			table_addr, IA32E_PDPT);
	table_present = check_page_table_present(map_params, table_entry);
	if (table_present == PT_NOT_PRESENT) {
		/* PDPTE not present, return PDPT base address */
		entry->entry_level  = IA32E_PDPT;
		entry->entry_base = (uint64_t)table_addr;
		entry->entry_present = PT_NOT_PRESENT;
		entry->page_size = check_mmu_1gb_support(map_params) ?
			(PAGE_SIZE_1G) : (PAGE_SIZE_2M);
		entry->entry_off = fetch_page_table_offset(addr, IA32E_PDPT);
		entry->entry_val =  table_entry;
		return;
	}
	if (table_entry & IA32E_PDPTE_PS_BIT) {
		/* 1GB page size, return the base addr of the pg entry*/
		entry->entry_level  = IA32E_PDPT;
		entry->entry_base = (uint64_t)table_addr;
		entry->page_size = check_mmu_1gb_support(map_params) ?
			(PAGE_SIZE_1G) : (PAGE_SIZE_2M);
		entry->entry_present = PT_PRESENT;
		entry->entry_off = fetch_page_table_offset(addr, IA32E_PDPT);
		entry->entry_val =  table_entry;
		return;
	}

	/* Obtain page table entry from PD table*/
	table_addr = (void *)(table_entry & IA32E_REF_MASK);
	table_entry = get_table_entry(map_params, addr,
			table_addr, IA32E_PD);
	table_present = check_page_table_present(map_params, table_entry);
	if (table_present == PT_NOT_PRESENT) {
		/* PDE not present, return PDE base address */
		entry->entry_level  = IA32E_PD;
		entry->entry_base = (uint64_t)table_addr;
		entry->entry_present = PT_NOT_PRESENT;
		entry->page_size  = PAGE_SIZE_2M;
		entry->entry_off = fetch_page_table_offset(addr, IA32E_PD);
		entry->entry_val =  table_entry;
		return;

	}
	if (table_entry & IA32E_PDE_PS_BIT) {
		/* 2MB page size, return the base addr of the pg entry*/
		entry->entry_level  = IA32E_PD;
		entry->entry_base = (uint64_t)table_addr;
		entry->entry_present = PT_PRESENT;
		entry->page_size  = PAGE_SIZE_2M;
		entry->entry_off = fetch_page_table_offset(addr, IA32E_PD);
		entry->entry_val =  table_entry;
		return;
	}

	/* Obtain page table entry from PT table*/
	table_addr = (void *)(table_entry & IA32E_REF_MASK);
	table_entry = get_table_entry(map_params, addr,
			table_addr, IA32E_PT);
	table_present = check_page_table_present(map_params, table_entry);
	entry->entry_present = ((table_present == PT_PRESENT)
			? (PT_PRESENT):(PT_NOT_PRESENT));
	entry->entry_level  = IA32E_PT;
	entry->entry_base = (uint64_t)table_addr;
	entry->page_size  = PAGE_SIZE_4K;
	entry->entry_off = fetch_page_table_offset(addr, IA32E_PT);
	entry->entry_val =  table_entry;
}

static uint64_t update_page_table_entry(struct map_params *map_params,
		void *paddr, void *vaddr, uint64_t size, uint64_t attr,
		enum mem_map_request_type request_type, bool direct)
{
	uint64_t remaining_size = size;
	uint32_t adjustment_size;
	int table_type = map_params->page_table_type;
	/* Obtain the PML4 address */
	void *table_addr = direct ? (map_params->pml4_base)
				: (map_params->pml4_inverted);

	/* Walk from the PML4 table to the PDPT table */
	table_addr = walk_paging_struct(vaddr, table_addr, IA32E_PML4,
			map_params);

	if ((remaining_size >= MEM_1G)
			&& (MEM_ALIGNED_CHECK(vaddr, MEM_1G))
			&& (MEM_ALIGNED_CHECK(paddr, MEM_1G))
			&& check_mmu_1gb_support(map_params)) {
		/* Map this 1 GByte memory region */
		adjustment_size = map_mem_region(vaddr, paddr,
				table_addr, attr, IA32E_PDPT,
				table_type, request_type);
	} else if ((remaining_size >= MEM_2M)
			&& (MEM_ALIGNED_CHECK(vaddr, MEM_2M))
			&& (MEM_ALIGNED_CHECK(paddr, MEM_2M))) {
		/* Walk from the PDPT table to the PD table */
		table_addr = walk_paging_struct(vaddr, table_addr,
				IA32E_PDPT, map_params);
		/* Map this 2 MByte memory region */
		adjustment_size = map_mem_region(vaddr, paddr,
				table_addr, attr, IA32E_PD, table_type,
				request_type);
	} else {
		/* Walk from the PDPT table to the PD table */
		table_addr = walk_paging_struct(vaddr,
				table_addr, IA32E_PDPT, map_params);
		/* Walk from the PD table to the page table */
		table_addr = walk_paging_struct(vaddr,
				table_addr, IA32E_PD, map_params);
		/* Map this 4 KByte memory region */
		adjustment_size = map_mem_region(vaddr, paddr,
				table_addr, attr, IA32E_PT,
				table_type, request_type);
	}

	return adjustment_size;

}

static uint64_t break_page_table(struct map_params *map_params, void *paddr,
		void *vaddr, uint64_t page_size, bool direct)
{
	uint32_t i = 0;
	uint64_t pa;
	uint64_t attr = 0x00;
	uint64_t next_page_size = 0x00;
	void *sub_tab_addr = NULL;
	struct entry_params entry;

	switch (page_size) {
	/* Breaking 1GB page to 2MB page*/
	case PAGE_SIZE_1G:
		next_page_size = PAGE_SIZE_2M;
		attr |= IA32E_PDE_PS_BIT;
		pr_info("%s, Breaking 1GB -->2MB vaddr=0x%llx",
				__func__, vaddr);
		break;

		/* Breaking 2MB page to 4KB page*/
	case PAGE_SIZE_2M:
		next_page_size = PAGE_SIZE_4K;
		pr_info("%s, Breaking 2MB -->4KB vaddr=0x%llx",
				__func__, vaddr);
		break;

		/* 4KB page, No action*/
	case PAGE_SIZE_4K:
	default:
		next_page_size = PAGE_SIZE_4K;
		pr_info("%s, Breaking 4KB no action vaddr=0x%llx",
				__func__, vaddr);
		break;
	}

	if (page_size != next_page_size) {
		obtain_last_page_table_entry(map_params, &entry, vaddr, direct);

		/* New entry present - need to allocate a new table */
		sub_tab_addr = alloc_paging_struct();
		/* Check to ensure memory available for this structure */
		if (sub_tab_addr == 0) {
			/* Error:
			 * Unable to find table memory necessary to map memory
			 */
			pr_err("Fail to find table memory for map memory");
			ASSERT(sub_tab_addr == 0, "");
			return 0;
		}

		/* the physical address maybe be not aligned of
		 * current page size, obtain the starting physical address
		 * aligned of current page size
		 */
		pa = ((((uint64_t)paddr) / page_size) * page_size);
		if (map_params->page_table_type == PTT_EPT) {
			/* Keep original attribute(here &0x3f)
			 * bit 0(R) bit1(W) bit2(X) bit3~5 MT
			 */
			attr |= (entry.entry_val & 0x3f);
		} else {
			/* Keep original attribute(here &0x7f) */
			attr |= (entry.entry_val & 0x7f);
		}
		/* write all entries and keep original attr*/
		for (i = 0; i < IA32E_NUM_ENTRIES; i++) {
			MEM_WRITE64(sub_tab_addr + (i * IA32E_COMM_ENTRY_SIZE),
					(attr | (pa + (i * next_page_size))));
		}
		if (map_params->page_table_type == PTT_EPT) {
			/* Write the table entry to map this memory,
			 * SDM chapter28 figure 28-1
			 * bit 0(R) bit1(W) bit2(X) bit3~5 MUST be reserved
			 * (here &0x07)
			 */
			MEM_WRITE64(entry.entry_base + entry.entry_off,
					((entry.entry_val & 0x07) |
					 ((uint64_t)sub_tab_addr)));
		} else {
			/* Write the table entry to map this memory,
			 * SDM chapter4 figure 4-11
			 * bit0(P) bit1(RW) bit2(U/S) bit3(PWT) bit4(PCD)
			 * bit5(A) bit6(D or Ignore)
			 */
			MEM_WRITE64(entry.entry_base + entry.entry_off,
					((entry.entry_val & 0x7f) |
					 ((uint64_t)sub_tab_addr)));
		}
	}

	return next_page_size;
}

static int modify_paging(struct map_params *map_params, void *paddr,
		void *vaddr, uint64_t size, uint32_t flags,
		enum mem_map_request_type request_type, bool direct)
{
	int64_t  remaining_size;
	uint64_t adjust_size;
	uint64_t attr;
	struct entry_params entry;
	uint64_t page_size;
	uint64_t vaddr_end = ((uint64_t)vaddr) + size;

	/* if the address is not PAGE aligned, will drop
	 * the unaligned part
	 */
	paddr = (void *)ROUND_PAGE_UP((uint64_t)paddr);
	vaddr = (void *)ROUND_PAGE_UP((uint64_t)vaddr);
	vaddr_end = ROUND_PAGE_DOWN(vaddr_end);
	remaining_size = vaddr_end - (uint64_t)vaddr;

	if ((request_type >= PAGING_REQUEST_TYPE_UNKNOWN)
			|| (map_params == NULL)) {
		pr_err("%s: vaddr=0x%llx size=0x%llx req_type=0x%lx",
			__func__, vaddr, size, request_type);
		ASSERT(0, "Incorrect Arguments");
		return -EINVAL;
	}

	attr = config_page_table_attr(map_params, flags);
	/* Loop until the entire block of memory is appropriately
	 * MAP/UNMAP/MODIFY
	 */
	while (remaining_size > 0) {
		obtain_last_page_table_entry(map_params, &entry, vaddr, direct);
		/* filter the unmap request, no action in this case*/
		page_size =  entry.page_size;
		if ((request_type == PAGING_REQUEST_TYPE_UNMAP)
				&& (entry.entry_present == PT_NOT_PRESENT)) {
			adjust_size =
				page_size - ((uint64_t)(vaddr) % page_size);
			vaddr += adjust_size;
			paddr += adjust_size;
			remaining_size -= adjust_size;
			continue;
		}

		/* if the address is NOT aligned of current page size,
		 * or required memory size < page size
		 * need to break page firstly
		 */
		if (entry.entry_present == PT_PRESENT) {
			/* Maybe need to recursive breaking in this case
			 * e.g. 1GB->2MB->4KB
			 */
			while ((uint64_t)remaining_size < page_size
				|| (!MEM_ALIGNED_CHECK(vaddr, page_size))
				|| (!MEM_ALIGNED_CHECK(paddr, page_size))) {
				/* The breaking function return the page size
				 * of next level page table
				 */
				page_size = break_page_table(map_params,
					paddr, vaddr, page_size, direct);
			}
		} else {
			page_size = ((uint64_t)remaining_size < page_size)
				? ((uint64_t)remaining_size) : (page_size);
		}
		/* The function return the memory size that one entry can map */
		adjust_size = update_page_table_entry(map_params, paddr, vaddr,
				page_size, attr, request_type, direct);
		vaddr += adjust_size;
		paddr += adjust_size;
		remaining_size -= adjust_size;
	}

	return 0;
}

int map_mem(struct map_params *map_params, void *paddr, void *vaddr,
		    uint64_t size, uint32_t flags)
{
	int ret = 0;

	/* used for MMU and EPT*/
	ret = modify_paging(map_params, paddr, vaddr, size, flags,
			PAGING_REQUEST_TYPE_MAP, true);
	if (ret < 0)
		return ret;
	/* only for EPT */
	if (map_params->page_table_type == PTT_EPT) {
		ret = modify_paging(map_params, vaddr, paddr, size, flags,
				PAGING_REQUEST_TYPE_MAP, false);
	}
	return ret;
}

int unmap_mem(struct map_params *map_params, void *paddr, void *vaddr,
		      uint64_t size, uint32_t flags)
{
	int ret = 0;

	/* used for MMU and EPT */
	ret = modify_paging(map_params, paddr, vaddr, size, flags,
			PAGING_REQUEST_TYPE_UNMAP, true);
	if (ret < 0)
		return ret;
	/* only for EPT */
	if (map_params->page_table_type == PTT_EPT) {
		ret = modify_paging(map_params, vaddr, paddr, size, flags,
				PAGING_REQUEST_TYPE_UNMAP, false);
	}
	return ret;
}

int modify_mem(struct map_params *map_params, void *paddr, void *vaddr,
		       uint64_t size, uint32_t flags)
{
	int ret = 0;

	/* used for MMU and EPT*/
	ret = modify_paging(map_params, paddr, vaddr, size, flags,
			PAGING_REQUEST_TYPE_MODIFY, true);
	if (ret < 0)
		return ret;
	/* only for EPT */
	if (map_params->page_table_type == PTT_EPT) {
		ret = modify_paging(map_params, vaddr, paddr, size, flags,
				PAGING_REQUEST_TYPE_MODIFY, false);
	}
	return ret;
}
