/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MMU_H
#define MMU_H

/* Size of all page-table entries (in bytes) */
#define     IA32E_COMM_ENTRY_SIZE           8

/* Definitions common for all IA-32e related paging entries */
#define     IA32E_COMM_P_BIT                0x0000000000000001
#define     IA32E_COMM_RW_BIT               0x0000000000000002
#define     IA32E_COMM_US_BIT               0x0000000000000004
#define     IA32E_COMM_PWT_BIT              0x0000000000000008
#define     IA32E_COMM_PCD_BIT              0x0000000000000010
#define     IA32E_COMM_A_BIT                0x0000000000000020
#define     IA32E_COMM_XD_BIT               0x8000000000000000

/* Defines for EPT paging entries */
#define     IA32E_EPT_R_BIT                 0x0000000000000001
#define     IA32E_EPT_W_BIT                 0x0000000000000002
#define     IA32E_EPT_X_BIT                 0x0000000000000004
#define     IA32E_EPT_UNCACHED              (0<<3)
#define     IA32E_EPT_WC                    (1<<3)
#define     IA32E_EPT_WT                    (4<<3)
#define     IA32E_EPT_WP                    (5<<3)
#define     IA32E_EPT_WB                    (6<<3)
#define     IA32E_EPT_PAT_IGNORE            0x0000000000000040
#define     IA32E_EPT_ACCESS_FLAG           0x0000000000000100
#define     IA32E_EPT_DIRTY_FLAG            0x0000000000000200
#define     IA32E_EPT_SNOOP_CTRL            0x0000000000000800
#define     IA32E_EPT_SUPPRESS_VE           0x8000000000000000

/* Definitions common or ignored for all IA-32e related paging entries */
#define     IA32E_COMM_D_BIT                0x0000000000000040
#define     IA32E_COMM_G_BIT                0x0000000000000100

/* Definitions exclusive to a Page Map Level 4 Entry (PML4E) */
#define     IA32E_PML4E_INDEX_MASK_START    39
#define     IA32E_PML4E_ADDR_MASK           0x0000FF8000000000

/* Definitions exclusive to a Page Directory Pointer Table Entry (PDPTE) */
#define     IA32E_PDPTE_D_BIT               0x0000000000000040
#define     IA32E_PDPTE_PS_BIT              0x0000000000000080
#define     IA32E_PDPTE_PAT_BIT             0x0000000000001000
#define     IA32E_PDPTE_ADDR_MASK           0x0000FFFFC0000000
#define     IA32E_PDPTE_INDEX_MASK_START  \
		(IA32E_PML4E_INDEX_MASK_START - IA32E_INDEX_MASK_BITS)

/* Definitions exclusive to a Page Directory Entry (PDE) 1G or 2M */
#define     IA32E_PDE_D_BIT                 0x0000000000000040
#define     IA32E_PDE_PS_BIT                0x0000000000000080
#define     IA32E_PDE_PAT_BIT               0x0000000000001000
#define     IA32E_PDE_ADDR_MASK             0x0000FFFFFFE00000
#define     IA32E_PDE_INDEX_MASK_START    \
		(IA32E_PDPTE_INDEX_MASK_START - IA32E_INDEX_MASK_BITS)

/* Definitions exclusive to Page Table Entries (PTE) */
#define     IA32E_PTE_D_BIT                 0x0000000000000040
#define     IA32E_PTE_PAT_BIT               0x0000000000000080
#define     IA32E_PTE_G_BIT                 0x0000000000000100
#define     IA32E_PTE_ADDR_MASK             0x0000FFFFFFFFF000
#define     IA32E_PTE_INDEX_MASK_START    \
		(IA32E_PDE_INDEX_MASK_START - IA32E_INDEX_MASK_BITS)

/** The 'Present' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_P       0x00000001
/** The 'Read/Write' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_RW      0x00000002
/** The 'User/Supervisor' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_US      0x00000004
/** The 'Page Write Through' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_PWT     0x00000008
/** The 'Page Cache Disable' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_PCD     0x00000010
/** The 'Accessed' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_A       0x00000020
/** The 'Dirty' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_D       0x00000040
/** The 'Page Size' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_PS      0x00000080
/** The 'Global' bit in a 32 bit paging page directory entry */
#define MMU_32BIT_PDE_G       0x00000100
/** The 'PAT' bit in a page 32 bit paging directory entry */
#define MMU_32BIT_PDE_PAT     0x00001000
/** The flag that indicates that the page fault was caused by a non present
 * page.
 */
#define PAGE_FAULT_P_FLAG    0x00000001
/** The flag that indicates that the page fault was caused by a write access. */
#define PAGE_FAULT_WR_FLAG   0x00000002
/** The flag that indicates that the page fault was caused in user mode. */
#define PAGE_FAULT_US_FLAG   0x00000004
/** The flag that indicates that the page fault was caused by a reserved bit
 * violation.
 */
#define PAGE_FAULT_RSVD_FLAG 0x00000008
/** The flag that indicates that the page fault was caused by an instruction
 * fetch.
 */
#define PAGE_FAULT_ID_FLAG   0x00000010

/* Defines used for common memory sizes */
#define             MEM_1K                   1024UL
#define             MEM_2K                  (MEM_1K * 2UL)
#define             MEM_4K                  (MEM_1K * 4UL)
#define             MEM_8K                  (MEM_1K * 8UL)
#define             MEM_16K                 (MEM_1K * 16UL)
#define             MEM_32K                 (MEM_1K * 32UL)
#define             MEM_64K                 (MEM_1K * 64UL)
#define             MEM_128K                (MEM_1K * 128UL)
#define             MEM_256K                (MEM_1K * 256UL)
#define             MEM_512K                (MEM_1K * 512UL)
#define             MEM_1M                  (MEM_1K * 1024UL)
#define             MEM_2M                  (MEM_1M * 2UL)
#define             MEM_4M                  (MEM_1M * 4UL)
#define             MEM_8M                  (MEM_1M * 8UL)
#define             MEM_16M                 (MEM_1M * 16UL)
#define             MEM_32M                 (MEM_1M * 32UL)
#define             MEM_64M                 (MEM_1M * 64UL)
#define             MEM_128M                (MEM_1M * 128UL)
#define             MEM_256M                (MEM_1M * 256UL)
#define             MEM_512M                (MEM_1M * 512UL)
#define             MEM_1G                  (MEM_1M * 1024UL)
#define             MEM_2G                  (MEM_1G * 2UL)
#define             MEM_3G                  (MEM_1G * 3UL)
#define             MEM_4G                  (MEM_1G * 4UL)
#define             MEM_5G                  (MEM_1G * 5UL)
#define             MEM_6G                  (MEM_1G * 6UL)

#ifndef ASSEMBLER

#include <cpu.h>

/* Define cache line size (in bytes) */
#define     CACHE_LINE_SIZE                 64

/* Size of all page structures for IA-32e */
#define     IA32E_STRUCT_SIZE               MEM_4K

/* IA32E Paging constants */
#define     IA32E_INDEX_MASK_BITS           9
#define     IA32E_NUM_ENTRIES               512
#define     IA32E_INDEX_MASK                (uint64_t)(IA32E_NUM_ENTRIES - 1)
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

/* Generic memory attributes */
#define     MMU_MEM_ATTR_READ                   0x00000001
#define     MMU_MEM_ATTR_WRITE                  0x00000002
#define     MMU_MEM_ATTR_EXECUTE                0x00000004
#define     MMU_MEM_ATTR_USER                   0x00000008
#define     MMU_MEM_ATTR_WB_CACHE               0x00000040
#define     MMU_MEM_ATTR_WT_CACHE               0x00000080
#define     MMU_MEM_ATTR_UNCACHED               0x00000100
#define     MMU_MEM_ATTR_WC                     0x00000200
#define     MMU_MEM_ATTR_WP                     0x00000400

/* Definitions for memory types related to x64 */
#define     MMU_MEM_ATTR_BIT_READ_WRITE         IA32E_COMM_RW_BIT
#define     MMU_MEM_ATTR_BIT_USER_ACCESSIBLE    IA32E_COMM_US_BIT
#define     MMU_MEM_ATTR_BIT_EXECUTE_DISABLE    IA32E_COMM_XD_BIT

/* Selection of Page Attribute Table (PAT) entries with PAT, PCD and PWT
 * encoding. See also pat.h
 */
/* Selects PAT0 WB  */
#define     MMU_MEM_ATTR_TYPE_CACHED_WB       (0x0000000000000000)
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

#define ROUND_PAGE_UP(addr)  (((addr) + CPU_PAGE_SIZE - 1) & CPU_PAGE_MASK)
#define ROUND_PAGE_DOWN(addr) ((addr) & CPU_PAGE_MASK)

struct map_params {
	/* enum _page_table_type: HOST or EPT*/
	int  page_table_type;
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

enum _page_table_type {
	PTT_HOST = 0,  /* Mapping for hypervisor */
	PTT_EPT = 1,
	PAGETABLE_TYPE_UNKNOWN,
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

/* Macros for reading/writing memory */
#define MEM_READ8(addr)        (*(volatile uint8_t *)(addr))
#define MEM_WRITE8(addr, data)  \
		(*(volatile uint8_t *)(addr) = (uint8_t)(data))
#define MEM_READ16(addr)       (*(volatile uint16_t *)(addr))
#define MEM_WRITE16(addr, data)  \
		(*(volatile uint16_t *)(addr) = (uint16_t)(data))
#define MEM_READ32(addr)       (*(volatile uint32_t *)(addr))
#define MEM_WRITE32(addr, data)  \
		(*(volatile uint32_t *)(addr) = (uint32_t)(data))
#define MEM_READ64(addr)       (*(volatile uint64_t *)(addr))
#define MEM_WRITE64(addr, data)  \
		(*(volatile uint64_t *)(addr) = (uint64_t)(data))

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
bool check_mmu_1gb_support(int page_table_type);
void *alloc_paging_struct(void);
void free_paging_struct(void *ptr);
void enable_paging(uint64_t pml4_base_addr);
void init_paging(void);
int map_mem(struct map_params *map_params, void *paddr, void *vaddr,
		    uint64_t size, uint32_t flags);
int unmap_mem(struct map_params *map_params, void *paddr, void *vaddr,
		      uint64_t size, uint32_t flags);
int modify_mem(struct map_params *map_params, void *paddr, void *vaddr,
		       uint64_t size, uint32_t flags);
int check_vmx_mmu_cap(void);
void invept(struct vcpu *vcpu);
bool check_continuous_hpa(struct vm *vm, uint64_t gpa, uint64_t size);
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
#define E820_TYPE_RAM           1	/* EFI 1, 2, 3, 4, 5, 6, 7 */
#define E820_TYPE_RESERVED      2
/* EFI 0, 11, 12, 13 (everything not used elsewhere) */
#define E820_TYPE_ACPI_RECLAIM  3	/* EFI 9 */
#define E820_TYPE_ACPI_NVS      4	/* EFI 10 */
#define E820_TYPE_UNUSABLE      5	/* EFI 8 */

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
	return pd + ((vaddr >> 22) + 1) * 1024;
}

#define CACHE_FLUSH_INVALIDATE_ALL()			\
{							\
	asm volatile ("   wbinvd\n" : : : "memory");	\
}

static inline void clflush(volatile void *p)
{
	asm volatile ("clflush (%0)" :: "r"(p));
}

/* External variable declarations */
extern uint8_t CPU_Boot_Page_Tables_Start_VM[];

/* External Interfaces */
int     is_ept_supported(void);
uint64_t create_guest_initial_paging(struct vm *vm);
void    destroy_ept(struct vm *vm);
uint64_t  gpa2hpa(struct vm *vm, uint64_t gpa);
uint64_t _gpa2hpa(struct vm *vm, uint64_t gpa, uint32_t *size);
uint64_t  hpa2gpa(struct vm *vm, uint64_t hpa);
int ept_mmap(struct vm *vm, uint64_t hpa,
	uint64_t gpa, uint64_t size, uint32_t type, uint32_t prot);

int     ept_violation_vmexit_handler(struct vcpu *vcpu);
int     ept_misconfig_vmexit_handler(struct vcpu *vcpu);
int     dm_emulate_mmio_post(struct vcpu *vcpu);

#endif /* ASSEMBLER not defined */

#endif /* MMU_H */
