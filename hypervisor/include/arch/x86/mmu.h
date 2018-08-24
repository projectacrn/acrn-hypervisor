/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MMU_H
#define MMU_H

/** The flag that indicates that the page fault was caused by a non present
 * page.
 */
#define PAGE_FAULT_P_FLAG	0x00000001U
/** The flag that indicates that the page fault was caused by a write access. */
#define PAGE_FAULT_WR_FLAG	0x00000002U
/** The flag that indicates that the page fault was caused in user mode. */
#define PAGE_FAULT_US_FLAG	0x00000004U
/** The flag that indicates that the page fault was caused by a reserved bit
 * violation.
 */
#define PAGE_FAULT_RSVD_FLAG	0x00000008U
/** The flag that indicates that the page fault was caused by an instruction
 * fetch.
 */
#define PAGE_FAULT_ID_FLAG	0x00000010U

/* Defines used for common memory sizes */
#define MEM_1K		1024U
#define MEM_2K		(MEM_1K * 2U)
#define MEM_4K		(MEM_1K * 4U)
#define MEM_1M		(MEM_1K * 1024U)
#define MEM_2M		(MEM_1M * 2U)
#define MEM_1G		(MEM_1M * 1024U)

#ifndef ASSEMBLER

#include <cpu.h>

/* Define cache line size (in bytes) */
#define CACHE_LINE_SIZE		64U

/* IA32E Paging constants */
#define IA32E_REF_MASK		\
		(boot_cpu_data.physical_address_mask)

#define ROUND_PAGE_UP(addr)	\
		((((addr) + (uint64_t)CPU_PAGE_SIZE) - 1UL) & CPU_PAGE_MASK)
#define ROUND_PAGE_DOWN(addr) ((addr) & CPU_PAGE_MASK)

enum _page_table_type {
	PTT_PRIMARY = 0,	/* Mapping for hypervisor */
	PTT_EPT = 1,
};

/* Represent the 4 levels of translation tables in IA-32e paging mode */
enum _page_table_level {
	IA32E_PML4 = 0,
	IA32E_PDPT = 1,
	IA32E_PD = 2,
	IA32E_PT = 3,
};

/* Page size */
#define PAGE_SIZE_4K	MEM_4K
#define PAGE_SIZE_2M	MEM_2M
#define PAGE_SIZE_1G	MEM_1G

uint64_t get_paging_pml4(void);
void *alloc_paging_struct(void);
void free_paging_struct(void *ptr);
void enable_paging(uint64_t pml4_base_addr);
void enable_smep(void);
void init_paging(void);
int mmu_add(uint64_t *pml4_page, uint64_t paddr_base,
		uint64_t vaddr_base, uint64_t size,
		uint64_t prot, enum _page_table_type ptt);
int mmu_modify_or_del(uint64_t *pml4_page,
		uint64_t vaddr_base, uint64_t size,
		uint64_t prot_set, uint64_t prot_clr,
		enum _page_table_type ptt, uint32_t type);
int check_vmx_mmu_cap(void);
uint16_t allocate_vpid(void);
void flush_vpid_single(uint16_t vpid);
void flush_vpid_global(void);
void invept(struct vcpu *vcpu);
bool check_continuous_hpa(struct vm *vm, uint64_t gpa_arg, uint64_t size_arg);
uint64_t *lookup_address(uint64_t *pml4_page, uint64_t addr,
		uint64_t *pg_size, enum _page_table_type ptt);

#pragma pack(1)

/** Defines a single entry in an E820 memory map. */
struct e820_entry {
   /** The base address of the memory range. */
	uint64_t baseaddr;
   /** The length of the memory range. */
	uint64_t length;
   /** The type of memory region. */
	uint32_t type;
};

#pragma pack()

/* E820 memory types */
#define E820_TYPE_RAM           1U	/* EFI 1, 2, 3, 4, 5, 6, 7 */
#define E820_TYPE_RESERVED      2U
/* EFI 0, 11, 12, 13 (everything not used elsewhere) */
#define E820_TYPE_ACPI_RECLAIM  3U	/* EFI 9 */
#define E820_TYPE_ACPI_NVS      4U	/* EFI 10 */
#define E820_TYPE_UNUSABLE      5U	/* EFI 8 */

/** Calculates the page table address for a given address.
 *
 * @param pd The base address of the page directory.
 * @param vaddr The virtual address to calculate the page table address for.
 *
 * @return A pointer to the page table for the specified virtual address.
 *
 */
static inline void *mmu_pt_for_pde(uint32_t *pd, uint32_t vaddr)
{
	return pd + (((vaddr >> 22U) + 1U) * 1024U);
}

#define CACHE_FLUSH_INVALIDATE_ALL()			\
{							\
	asm volatile ("   wbinvd\n" : : : "memory");	\
}

static inline void clflush(volatile void *p)
{
	asm volatile ("clflush (%0)" :: "r"(p));
}

/* External Interfaces */
void    destroy_ept(struct vm *vm);
uint64_t  gpa2hpa(const struct vm *vm, uint64_t gpa);
uint64_t  local_gpa2hpa(const struct vm *vm, uint64_t gpa, uint32_t *size);
uint64_t  hpa2gpa(const struct vm *vm, uint64_t hpa);
int ept_mr_add(const struct vm *vm, uint64_t *pml4_page, uint64_t hpa,
	uint64_t gpa, uint64_t size, uint64_t prot_orig);
int ept_mr_modify(const struct vm *vm, uint64_t *pml4_page,
	uint64_t gpa, uint64_t size,
	uint64_t prot_set, uint64_t prot_clr);
int ept_mr_del(const struct vm *vm, uint64_t *pml4_page,
	uint64_t gpa, uint64_t size);
void free_ept_mem(uint64_t *pml4_page);
int     ept_violation_vmexit_handler(struct vcpu *vcpu);
int     ept_misconfig_vmexit_handler(__unused struct vcpu *vcpu);

#endif /* ASSEMBLER not defined */

#endif /* MMU_H */
