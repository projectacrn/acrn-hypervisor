/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file mmu.h
 *
 * @brief APIs for Memory Management module
 */
#ifndef MMU_H
#define MMU_H
/**
 * @brief Memory Management
 *
 * @defgroup acrn_mem ACRN Memory Management
 * @{
 */
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
#include <page.h>
#include <pgtable.h>

/* Define cache line size (in bytes) */
#define CACHE_LINE_SIZE		64U

/* IA32E Paging constants */
#define IA32E_REF_MASK	((get_cpu_info())->physical_address_mask)

struct acrn_vcpu;
static inline uint64_t round_page_up(uint64_t addr)
{
	return (((addr + (uint64_t)PAGE_SIZE) - 1UL) & PAGE_MASK);
}

static inline uint64_t round_page_down(uint64_t addr)
{
	return (addr & PAGE_MASK);
}

static inline uint64_t round_pde_up(uint64_t val)
{
	return (((val + (uint64_t)PDE_SIZE) - 1UL) & PDE_MASK);
}

static inline uint64_t round_pde_down(uint64_t val)
{
	return (val & PDE_MASK);
}

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

/* Page size */
#define PAGE_SIZE_4K	MEM_4K
#define PAGE_SIZE_2M	MEM_2M
#define PAGE_SIZE_1G	MEM_1G

void sanitize_pte_entry(uint64_t *ptep);
void sanitize_pte(uint64_t *pt_page);
/**
 * @brief MMU paging enable
 *
 * @return None
 */
void enable_paging(void);
/**
 * @brief Supervisor-mode execution prevention (SMEP) enable
 *
 * @return None
 */
void enable_smep(void);

/**
 * @brief Supervisor-mode Access Prevention (SMAP) enable
 *
 * @return None
 */
void enable_smap(void);

/**
 * @brief MMU page tables initialization
 *
 * @return None
 */
void init_paging(void);
void mmu_add(uint64_t *pml4_page, uint64_t paddr_base, uint64_t vaddr_base,
		uint64_t size, uint64_t prot, const struct memory_ops *mem_ops);
void mmu_modify_or_del(uint64_t *pml4_page, uint64_t vaddr_base, uint64_t size,
		uint64_t prot_set, uint64_t prot_clr, const struct memory_ops *mem_ops, uint32_t type);
void hv_access_memory_region_update(uint64_t base, uint64_t size);

/**
 * @brief VPID allocation
 *
 * @retval 0 VPID overflow
 * @retval >0 the valid VPID
 */
uint16_t allocate_vpid(void);
/**
 * @brief Specified signle VPID flush
 *
 * @param[in] vpid the specified VPID
 *
 * @return None
 */
void flush_vpid_single(uint16_t vpid);
/**
 * @brief All VPID flush
 *
 * @return None
 */
void flush_vpid_global(void);
/**
 * @brief Guest-physical mappings and combined mappings invalidation
 *
 * @param[in] vcpu the pointer that points the vcpu data structure
 *
 * @return None
 */
void invept(const struct acrn_vcpu *vcpu);

static inline void cache_flush_invalidate_all(void)
{
	asm volatile ("   wbinvd\n" : : : "memory");
}

static inline void clflush(volatile void *p)
{
	asm volatile ("clflush (%0)" :: "r"(p));
}

/* get PDPT address from CR3 vaule in PAE mode */
static inline uint64_t get_pae_pdpt_addr(uint64_t cr3)
{
	return (cr3 & 0xFFFFFFE0UL);
}

/**
 * @}
 */
#endif /* ASSEMBLER not defined */

#endif /* MMU_H */
