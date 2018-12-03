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

/* Define cache line size (in bytes) */
#define CACHE_LINE_SIZE		64U

/* IA32E Paging constants */
#define IA32E_REF_MASK		\
		(boot_cpu_data.physical_address_mask)

static inline uint64_t round_page_up(uint64_t addr)
{
	return (((addr + (uint64_t)PAGE_SIZE) - 1UL) & PAGE_MASK);
}

static inline uint64_t round_page_down(uint64_t addr)
{
	return (addr & PAGE_MASK);
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
 * @brief MMU page tables initialization
 *
 * @return None
 */
void init_paging(void);
void mmu_add(uint64_t *pml4_page, uint64_t paddr_base, uint64_t vaddr_base,
		uint64_t size, uint64_t prot, const struct memory_ops *mem_ops);
void mmu_modify_or_del(uint64_t *pml4_page, uint64_t vaddr_base, uint64_t size,
		uint64_t prot_set, uint64_t prot_clr, const struct memory_ops *mem_ops, uint32_t type);
/**
 * @brief EPT and VPID capability checking
 *
 * @retval 0 on success
 * @retval -ENODEV Don't support EPT or VPID capability
 */
int check_vmx_mmu_cap(void);
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
/**
 *@pre (pml4_page != NULL) && (pg_size != NULL)
 */
uint64_t *lookup_address(uint64_t *pml4_page, uint64_t addr,
		uint64_t *pg_size, const struct memory_ops *mem_ops);

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
#define E820_TYPE_RAM		1U	/* EFI 1, 2, 3, 4, 5, 6, 7 */
#define E820_TYPE_RESERVED	2U
/* EFI 0, 11, 12, 13 (everything not used elsewhere) */
#define E820_TYPE_ACPI_RECLAIM	3U	/* EFI 9 */
#define E820_TYPE_ACPI_NVS	4U	/* EFI 10 */
#define E820_TYPE_UNUSABLE	5U	/* EFI 8 */

static inline void cache_flush_invalidate_all(void)
{
	asm volatile ("   wbinvd\n" : : : "memory");
}

static inline void clflush(volatile void *p)
{
	asm volatile ("clflush (%0)" :: "r"(p));
}

/**
 * Invalid HPA is defined for error checking,
 * according to SDM vol.3A 4.1.4, the maximum
 * host physical address width is 52
 */
#define INVALID_HPA	(0x1UL << 52U)
#define INVALID_GPA	(0x1UL << 52U)
/* External Interfaces */
/**
 * @brief EPT page tables destroy
 *
 * @param[inout] vm the pointer that points to VM data structure
 *
 * @return None
 */
void destroy_ept(struct acrn_vm *vm);
/**
 * @brief Translating from guest-physical address to host-physcial address
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] gpa the specified guest-physical address
 *
 * @retval hpa the host physical address mapping to the \p gpa
 * @retval INVALID_HPA the HPA of parameter gpa is unmapping
 */
uint64_t gpa2hpa(struct acrn_vm *vm, uint64_t gpa);
/**
 * @brief Translating from guest-physical address to host-physcial address
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] gpa the specified guest-physical address
 * @param[out] size the pointer that returns the page size of
 *                  the page in which the gpa is
 *
 * @retval hpa the host physical address mapping to the \p gpa
 * @retval INVALID_HPA the HPA of parameter gpa is unmapping
 */
uint64_t local_gpa2hpa(struct acrn_vm *vm, uint64_t gpa, uint32_t *size);
/**
 * @brief Translating from host-physical address to guest-physical address for VM0
 *
 * @param[in] hpa the specified host-physical address
 *
 * @pre: the gpa and hpa are identical mapping in SOS.
 */
uint64_t vm0_hpa2gpa(uint64_t hpa);
/**
 * @brief Guest-physical memory region mapping
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] pml4_page The physical address of The EPTP
 * @param[in] hpa The specified start host physical address of host
 *                physical memory region that GPA will be mapped
 * @param[in] gpa The specified start guest physical address of guest
 *                physical memory region that needs to be mapped
 * @param[in] size The size of guest physical memory region that needs
 *                 to be mapped
 * @param[in] prot_orig The specified memory access right and memory type
 *
 * @return None
 */
void ept_mr_add(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t hpa,
		uint64_t gpa, uint64_t size, uint64_t prot_orig);
/**
 * @brief Guest-physical memory page access right or memory type updating
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] pml4_page The physical address of The EPTP
 * @param[in] gpa The specified start guest physical address of guest
 *            physical memory region whoes mapping needs to be updated
 * @param[in] size The size of guest physical memory region
 * @param[in] prot_set The specified memory access right and memory type
 *                     that will be set
 * @param[in] prot_clr The specified memory access right and memory type
 *                     that will be cleared
 *
 * @return None
 */
void ept_mr_modify(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t gpa,
		uint64_t size, uint64_t prot_set, uint64_t prot_clr);
/**
 * @brief Guest-physical memory region unmapping
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] pml4_page The physical address of The EPTP
 * @param[in] gpa The specified start guest physical address of guest
 *                physical memory region whoes mapping needs to be deleted
 * @param[in] size The size of guest physical memory region
 *
 * @return None
 *
 * @pre [gpa,gpa+size) has been mapped into host physical memory region
 */
void ept_mr_del(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t gpa,
		uint64_t size);
/**
 * @brief EPT violation handling
 *
 * @param[in] vcpu the pointer that points to vcpu data structure
 *
 * @retval -EINVAL fail to handle the EPT violation
 * @retval 0 Success to handle the EPT violation
 */
int ept_violation_vmexit_handler(struct acrn_vcpu *vcpu);
/**
 * @brief EPT misconfiguration handling
 *
 * @param[in] vcpu the pointer that points to vcpu data structure
 *
 * @retval -EINVAL fail to handle the EPT misconfig
 * @retval 0 Success to handle the EPT misconfig
 */
int ept_misconfig_vmexit_handler(__unused struct acrn_vcpu *vcpu);

/**
 * @}
 */
#endif /* ASSEMBLER not defined */

#endif /* MMU_H */
