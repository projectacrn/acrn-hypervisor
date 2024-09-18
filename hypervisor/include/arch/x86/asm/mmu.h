/*
 * Copyright (C) 2018-2022 Intel Corporation.
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
#define MEM_2G		(MEM_1G * 2UL)
#define MEM_4G		(MEM_1G * 4UL)

#ifndef ASSEMBLER

#include <asm/page.h>
#include <asm/pgtable.h>

/* Define cache line size (in bytes) */
#define CACHE_LINE_SIZE		64U

/* IA32E Paging constants */
#define IA32E_REF_MASK	((get_pcpu_info())->physical_address_mask)
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

struct invept_desc {
	uint64_t eptp;
	uint64_t res;
};

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

/* Page size */
#define PAGE_SIZE_4K	MEM_4K
#define PAGE_SIZE_2M	MEM_2M
#define PAGE_SIZE_1G	MEM_1G

/**
 * @brief MMU paging enable
 */
void enable_paging(void);
/**
 * @brief Supervisor-mode execution prevention (SMEP) enable
 */
void enable_smep(void);

/**
 * @brief Supervisor-mode Access Prevention (SMAP) enable
 */
void enable_smap(void);

/**
 * @brief MMU page tables initialization
 */
void init_paging(void);

/*
 * set paging attribute for primary page tables
 */
void set_paging_supervisor(uint64_t base, uint64_t size);
void set_paging_x(uint64_t base, uint64_t size);
void set_paging_nx(uint64_t base, uint64_t size);

/**
 * @brief Specified signle VPID flush
 *
 * @param[in] vpid the specified VPID
 */
void flush_vpid_single(uint16_t vpid);
/**
 * @brief All VPID flush
 */
void flush_vpid_global(void);

/**
 * @brief Guest-physical mappings and combined mappings invalidation
 *
 * @param[in] eptp the pointer that points the eptp
 */
void invept(const void *eptp);

/* get PDPT address from CR3 vaule in PAE mode */
static inline uint64_t get_pae_pdpt_addr(uint64_t cr3)
{
	return (cr3 & 0xFFFFFFE0UL);
}

/*
 * flush TLB only for the specified page with the address
 */
void flush_tlb(uint64_t addr);
void flush_tlb_range(uint64_t addr, uint64_t size);

void flush_invalidate_all_cache(void);
void flush_cacheline(const volatile void *p);
void flush_cache_range(const volatile void *p, uint64_t size);
void allocate_ppt_pages(void);

/**
 * @}
 */
#endif /* ASSEMBLER not defined */

#endif /* MMU_H */
