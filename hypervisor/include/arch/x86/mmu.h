/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MMU_H
#define MMU_H

/* Size of all page-table entries (in bytes) */
#define     IA32E_COMM_ENTRY_SIZE           8U

/* Definitions common for all IA-32e related paging entries */
#define     IA32E_COMM_P_BIT                0x0000000000000001UL
#define     IA32E_COMM_RW_BIT               0x0000000000000002UL
#define     IA32E_COMM_US_BIT               0x0000000000000004UL
#define     IA32E_COMM_PWT_BIT              0x0000000000000008UL
#define     IA32E_COMM_PCD_BIT              0x0000000000000010UL
#define     IA32E_COMM_A_BIT                0x0000000000000020UL
#define     IA32E_COMM_XD_BIT               0x8000000000000000UL

/* Defines for EPT paging entries */
#define     IA32E_EPT_R_BIT                 0x0000000000000001UL
#define     IA32E_EPT_W_BIT                 0x0000000000000002UL
#define     IA32E_EPT_X_BIT                 0x0000000000000004UL
#define     IA32E_EPT_UNCACHED              (0UL<<3)
#define     IA32E_EPT_WC                    (1UL<<3)
#define     IA32E_EPT_WT                    (4UL<<3)
#define     IA32E_EPT_WP                    (5UL<<3)
#define     IA32E_EPT_WB                    (6UL<<3)
#define     IA32E_EPT_MT_MASK               (7UL<<3)
#define     IA32E_EPT_PAT_IGNORE            0x0000000000000040UL
#define     IA32E_EPT_ACCESS_FLAG           0x0000000000000100UL
#define     IA32E_EPT_DIRTY_FLAG            0x0000000000000200UL
#define     IA32E_EPT_SNOOP_CTRL            0x0000000000000800UL
#define     IA32E_EPT_SUPPRESS_VE           0x8000000000000000UL

/* Definitions common or ignored for all IA-32e related paging entries */
#define     IA32E_COMM_D_BIT                0x0000000000000040UL
#define     IA32E_COMM_G_BIT                0x0000000000000100UL

/* Definitions exclusive to a Page Map Level 4 Entry (PML4E) */
#define     IA32E_PML4E_INDEX_MASK_START    39
#define     IA32E_PML4E_ADDR_MASK           0x0000FF8000000000UL

/* Definitions exclusive to a Page Directory Pointer Table Entry (PDPTE) */
#define     IA32E_PDPTE_D_BIT               0x0000000000000040UL
#define     IA32E_PDPTE_PS_BIT              0x0000000000000080UL
#define     IA32E_PDPTE_PAT_BIT             0x0000000000001000UL
#define     IA32E_PDPTE_ADDR_MASK           0x0000FFFFC0000000UL
#define     IA32E_PDPTE_INDEX_MASK_START  \
		(IA32E_PML4E_INDEX_MASK_START - IA32E_INDEX_MASK_BITS)

/* Definitions exclusive to a Page Directory Entry (PDE) 1G or 2M */
#define     IA32E_PDE_D_BIT                 0x0000000000000040UL
#define     IA32E_PDE_PS_BIT                0x0000000000000080UL
#define     IA32E_PDE_PAT_BIT               0x0000000000001000UL
#define     IA32E_PDE_ADDR_MASK             0x0000FFFFFFE00000UL
#define     IA32E_PDE_INDEX_MASK_START    \
		(IA32E_PDPTE_INDEX_MASK_START - IA32E_INDEX_MASK_BITS)

/* Definitions exclusive to Page Table Entries (PTE) */
#define     IA32E_PTE_D_BIT                 0x0000000000000040UL
#define     IA32E_PTE_PAT_BIT               0x0000000000000080UL
#define     IA32E_PTE_G_BIT                 0x0000000000000100UL
#define     IA32E_PTE_ADDR_MASK             0x0000FFFFFFFFF000UL
#define     IA32E_PTE_INDEX_MASK_START    \
		(IA32E_PDE_INDEX_MASK_START - IA32E_INDEX_MASK_BITS)

/** The 'Present' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_P       0x00000001U
/** The 'Read/Write' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_RW      0x00000002U
/** The 'User/Supervisor' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_US      0x00000004U
/** The 'Page Write Through' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_PWT     0x00000008U
/** The 'Page Cache Disable' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_PCD     0x00000010U
/** The 'Accessed' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_A       0x00000020U
/** The 'Dirty' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_D       0x00000040U
/** The 'Page Size' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_PS      0x00000080U
/** The 'Global' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_G       0x00000100U
/** The 'PAT' bit in a page 32 bit paging directory entry */
#define MMU_32BIT_PDE_PAT     0x00001000U
/** The flag that indicates that the page fault was caused by a non present
 * page.
 */
#define PAGE_FAULT_P_FLAG    0x00000001U
/** The flag that indicates that the page fault was caused by a write access. */
#define PAGE_FAULT_WR_FLAG   0x00000002U
/** The flag that indicates that the page fault was caused in user mode. */
#define PAGE_FAULT_US_FLAG   0x00000004U
/** The flag that indicates that the page fault was caused by a reserved bit
 * violation.
 */
#define PAGE_FAULT_RSVD_FLAG 0x00000008U
/** The flag that indicates that the page fault was caused by an instruction
 * fetch.
 */
#define PAGE_FAULT_ID_FLAG   0x00000010U

/* Defines used for common memory sizes */
#define             MEM_1K                   1024U
#define             MEM_2K                  (MEM_1K * 2U)
#define             MEM_4K                  (MEM_1K * 4U)
#define             MEM_8K                  (MEM_1K * 8U)
#define             MEM_16K                 (MEM_1K * 16U)
#define             MEM_32K                 (MEM_1K * 32U)
#define             MEM_64K                 (MEM_1K * 64U)
#define             MEM_128K                (MEM_1K * 128U)
#define             MEM_256K                (MEM_1K * 256U)
#define             MEM_512K                (MEM_1K * 512U)
#define             MEM_1M                  (MEM_1K * 1024U)
#define             MEM_2M                  (MEM_1M * 2U)
#define             MEM_4M                  (MEM_1M * 4U)
#define             MEM_8M                  (MEM_1M * 8U)
#define             MEM_16M                 (MEM_1M * 16U)
#define             MEM_32M                 (MEM_1M * 32U)
#define             MEM_64M                 (MEM_1M * 64U)
#define             MEM_128M                (MEM_1M * 128U)
#define             MEM_256M                (MEM_1M * 256U)
#define             MEM_512M                (MEM_1M * 512U)
#define             MEM_1G                  (MEM_1M * 1024U)
#define             MEM_2G                  (MEM_1G * 2U)
#define             MEM_3G                  (MEM_1G * 3U)
#define             MEM_4G                  (MEM_1G * 4U)
#define             MEM_5G                  (MEM_1G * 5U)
#define             MEM_6G                  (MEM_1G * 6U)

#ifndef ASSEMBLER

#include <cpu.h>

/* Define cache line size (in bytes) */
#define     CACHE_LINE_SIZE                 64U

/* Size of all page structures for IA-32e */
#define     IA32E_STRUCT_SIZE               MEM_4K

/* IA32E Paging constants */
#define     IA32E_INDEX_MASK_BITS           9
#define     IA32E_NUM_ENTRIES               512U
#define     IA32E_INDEX_MASK                (uint64_t)(IA32E_NUM_ENTRIES - 1U)
#define     IA32E_REF_MASK			\
		(boot_cpu_data.physical_address_mask)
#define     IA32E_FIRST_BLOCK_INDEX         1

/* Macro to get PML4 index given an address */
#define     IA32E_PML4E_INDEX_CALC(address)                 \
	(uint32_t)((((uint64_t)address >> IA32E_PML4E_INDEX_MASK_START) & \
		IA32E_INDEX_MASK) * sizeof(uint64_t))

/* Macro to get PDPT index given an address */
#define     IA32E_PDPTE_INDEX_CALC(address)                 \
	(uint32_t)((((uint64_t)address >> IA32E_PDPTE_INDEX_MASK_START) & \
		IA32E_INDEX_MASK) * sizeof(uint64_t))

/* Macro to get PD index given an address */
#define     IA32E_PDE_INDEX_CALC(address)                   \
	(uint32_t)((((uint64_t)address >> IA32E_PDE_INDEX_MASK_START) & \
		IA32E_INDEX_MASK) * sizeof(uint64_t))

/* Macro to get PT index given an address */
#define     IA32E_PTE_INDEX_CALC(address)                   \
	(uint32_t)((((uint64_t)address >> IA32E_PTE_INDEX_MASK_START) & \
		IA32E_INDEX_MASK) * sizeof(uint64_t))

/* Macro to obtain a 2 MB page offset from given linear address */
#define     IA32E_GET_2MB_PG_OFFSET(address)                \
		(address & 0x001FFFFF)

/* Macro to obtain a 4KB page offset from given linear address */
#define     IA32E_GET_4KB_PG_OFFSET(address)                \
		(address & 0x00000FFF)

/*
 * The following generic attributes MMU_MEM_ATTR_FLAG_xxx may be OR'd with one
 * and only one of the MMU_MEM_ATTR_TYPE_xxx definitions
 */

/* Definitions for memory types related to x64 */
#define     MMU_MEM_ATTR_BIT_READ_WRITE         IA32E_COMM_RW_BIT
#define     MMU_MEM_ATTR_BIT_USER_ACCESSIBLE    IA32E_COMM_US_BIT
#define     MMU_MEM_ATTR_BIT_EXECUTE_DISABLE    IA32E_COMM_XD_BIT

/* Selection of Page Attribute Table (PAT) entries with PAT, PCD and PWT
 * encoding. See also pat.h
 */
/* Selects PAT0 WB  */
#define     MMU_MEM_ATTR_TYPE_CACHED_WB       (0x0000000000000000UL)
/* Selects PAT1 WT  */
#define     MMU_MEM_ATTR_TYPE_CACHED_WT       (IA32E_COMM_PWT_BIT)
/* Selects PAT2 UCM */
#define     MMU_MEM_ATTR_TYPE_UNCACHED_MINUS  (IA32E_COMM_PCD_BIT)
/* Selects PAT3 UC  */
#define     MMU_MEM_ATTR_TYPE_UNCACHED        \
		(IA32E_COMM_PCD_BIT | IA32E_COMM_PWT_BIT)
/* Selects PAT6 WC  */
#define     MMU_MEM_ATTR_TYPE_WRITE_COMBINED  \
		(IA32E_PDPTE_PAT_BIT | IA32E_COMM_PCD_BIT)
/* Selects PAT7 WP  */
#define     MMU_MEM_ATTR_TYPE_WRITE_PROTECTED \
		(IA32E_PDPTE_PAT_BIT | IA32E_COMM_PCD_BIT | IA32E_COMM_PWT_BIT)
/* memory type bits mask */
#define     MMU_MEM_ATTR_TYPE_MASK \
		(IA32E_PDPTE_PAT_BIT | IA32E_COMM_PCD_BIT | IA32E_COMM_PWT_BIT)

#define ROUND_PAGE_UP(addr)  \
		((((addr) + (uint64_t)CPU_PAGE_SIZE) - 1UL) & CPU_PAGE_MASK)
#define ROUND_PAGE_DOWN(addr) ((addr) & CPU_PAGE_MASK)

enum _page_table_type {
	PTT_HOST = 0,  /* Mapping for hypervisor */
	PTT_EPT = 1,
	PAGETABLE_TYPE_UNKNOWN,
};

struct map_params {
	/* enum _page_table_type: HOST or EPT*/
	enum _page_table_type page_table_type;
	/* used HVA->HPA for HOST, used GPA->HPA for EPT */
	void *pml4_base;
	/* used HPA->HVA for HOST, used HPA->GPA for EPT */
	void *pml4_inverted;
};
struct entry_params {
	uint32_t entry_level;
	uint32_t entry_present;
	void *entry_base;
	uint64_t entry_off;
	uint64_t entry_val;
	uint64_t page_size;
};

/* Represent the 4 levels of translation tables in IA-32e paging mode */
enum _page_table_level {
	IA32E_PML4 = 0,
	IA32E_PDPT = 1,
	IA32E_PD = 2,
	IA32E_PT = 3,
	IA32E_UNKNOWN,
};

/* Page table entry present */
enum _page_table_present {
	PT_NOT_PRESENT = 0,
	PT_PRESENT = 1,
	PT_MISCFG_PRESENT = 2,
};

/* Page size */
#define PAGE_SIZE_4K	MEM_4K
#define PAGE_SIZE_2M	MEM_2M
#define PAGE_SIZE_1G	MEM_1G

/* Inline functions for reading/writing memory */
static inline uint8_t mem_read8(void *addr)
{
	return *(volatile uint8_t *)(addr);
}

static inline uint16_t mem_read16(void *addr)
{
	return *(volatile uint16_t *)(addr);
}

static inline uint32_t mem_read32(void *addr)
{
	return *(volatile uint32_t *)(addr);
}

static inline uint64_t mem_read64(void *addr)
{
	return *(volatile uint64_t *)(addr);
}

static inline void mem_write8(void *addr, uint8_t data)
{
	volatile uint8_t *addr8 = (volatile uint8_t *)addr;
	*addr8 = data;
}

static inline void mem_write16(void *addr, uint16_t data)
{
	volatile uint16_t *addr16 = (volatile uint16_t *)addr;
	*addr16 = data;
}

static inline void mem_write32(void *addr, uint32_t data)
{
	volatile uint32_t *addr32 = (volatile uint32_t *)addr;
	*addr32 = data;
}

static inline void mem_write64(void *addr, uint64_t data)
{
	volatile uint64_t *addr64 = (volatile uint64_t *)addr;
	*addr64 = data;
}

/* Typedef for MMIO handler and range check routine */
typedef int(*hv_mem_io_handler_t)(struct vcpu *, struct mem_io *, void *);

/* Structure for MMIO handler node */
struct mem_io_node {
	hv_mem_io_handler_t read_write;
	void *handler_private_data;
	struct list_head list;
	uint64_t range_start;
	uint64_t range_end;
};

uint64_t get_paging_pml4(void);
bool check_mmu_1gb_support(enum _page_table_type page_table_type);
void *alloc_paging_struct(void);
void free_paging_struct(void *ptr);
void enable_paging(uint64_t pml4_base_addr);
void enable_smep(void);
void init_paging(void);
int map_mem(struct map_params *map_params, void *paddr, void *vaddr,
		    uint64_t size, uint32_t flags);
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
int obtain_last_page_table_entry(struct map_params *map_params,
		struct entry_params *entry, void *addr, bool direct);

int register_mmio_emulation_handler(struct vm *vm,
	hv_mem_io_handler_t read_write, uint64_t start,
	uint64_t end, void *handler_private_data);

void unregister_mmio_emulation_handler(struct vm *vm, uint64_t start,
        uint64_t end);

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
bool is_ept_supported(void);
uint64_t create_guest_initial_paging(struct vm *vm);
void    destroy_ept(struct vm *vm);
uint64_t  gpa2hpa(struct vm *vm, uint64_t gpa);
uint64_t _gpa2hpa(struct vm *vm, uint64_t gpa, uint32_t *size);
uint64_t  hpa2gpa(struct vm *vm, uint64_t hpa);
int ept_mr_add(struct vm *vm, uint64_t hpa_arg,
	uint64_t gpa_arg, uint64_t size, uint32_t prot_arg);
int ept_mr_modify(struct vm *vm, uint64_t *pml4_page,
	uint64_t gpa, uint64_t size,
	uint64_t prot_set, uint64_t prot_clr);
int ept_mr_del(struct vm *vm, uint64_t *pml4_page,
	uint64_t gpa, uint64_t size);

int     ept_violation_vmexit_handler(struct vcpu *vcpu);
int     ept_misconfig_vmexit_handler(struct vcpu *vcpu);
int     dm_emulate_mmio_post(struct vcpu *vcpu);

#endif /* ASSEMBLER not defined */

#endif /* MMU_H */
