/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <bsp_extern.h>
#include <multiboot.h>

#define ACRN_DBG_GUEST	6

/* for VM0 e820 */
uint32_t e820_entries;
struct e820_entry e820[E820_MAX_ENTRIES];
struct e820_mem_params e820_mem;

struct page_walk_info {
	uint64_t top_entry;	/* Top level paging structure entry */
	int level;
	int width;
	bool is_user_mode;
	bool is_write_access;
	bool is_inst_fetch;
	bool pse;		/* CR4.PSE for 32bit paing,
				 * true for PAE/4-level paing */
	bool wp;		/* CR0.WP */
	bool nxe;		/* MSR_IA32_EFER_NXE_BIT */
};

inline bool
is_vm0(struct vm *vm)
{
	return (vm->attr.boot_idx & 0x7F) == 0;
}

inline struct vcpu *vcpu_from_vid(struct vm *vm, int vcpu_id)
{
	int i;
	struct vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		if (vcpu->vcpu_id == vcpu_id)
			return vcpu;
	}

	return NULL;
}

inline struct vcpu *vcpu_from_pid(struct vm *vm, int pcpu_id)
{
	int i;
	struct vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		if (vcpu->pcpu_id == pcpu_id)
			return vcpu;
	}

	return NULL;
}

inline struct vcpu *get_primary_vcpu(struct vm *vm)
{
	int i;
	struct vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		if (is_vcpu_bsp(vcpu))
			return vcpu;
	}

	return NULL;
}

inline uint64_t vcpumask2pcpumask(struct vm *vm, uint64_t vdmask)
{
	int vcpu_id;
	uint64_t dmask = 0;
	struct vcpu *vcpu;

	while ((vcpu_id = ffs64(vdmask)) >= 0) {
		bitmap_clear(vcpu_id, &vdmask);
		vcpu = vcpu_from_vid(vm, vcpu_id);
		ASSERT(vcpu, "vcpu_from_vid failed");
		bitmap_set(vcpu->pcpu_id, &dmask);
	}

	return dmask;
}

inline bool vm_lapic_disabled(struct vm *vm)
{
	int i;
	struct vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		if (vlapic_enabled(vcpu->arch_vcpu.vlapic))
			return false;
	}

	return true;
}

/* Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continous
 */
int copy_from_vm(struct vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	uint64_t hpa;
	uint32_t off_in_pg, len, pg_size;
	void *g_ptr;

	do {
		hpa = _gpa2hpa(vm, gpa, &pg_size);
		if (pg_size == 0) {
			ASSERT(0, "copy_from_vm: GPA2HPA not found");
			return -EINVAL;
		}

		off_in_pg = gpa & (pg_size - 1);
		if (size > pg_size - off_in_pg)
			len = pg_size - off_in_pg;
		else
			len = size;

		g_ptr = HPA2HVA(hpa);
		memcpy_s(h_ptr, len, g_ptr, len);
		gpa += len;
		h_ptr += len;
		size -= len;
	} while (size > 0);

	return 0;
}

int copy_to_vm(struct vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	uint64_t hpa;
	uint32_t off_in_pg, len, pg_size;
	void *g_ptr;

	do {
		hpa = _gpa2hpa(vm, gpa, &pg_size);
		if (pg_size == 0) {
			ASSERT(0, "copy_to_vm: GPA2HPA not found");
			return -EINVAL;
		}

		off_in_pg = gpa & (pg_size - 1);
		if (size > pg_size - off_in_pg)
			len = pg_size - off_in_pg;
		else
			len = size;

		g_ptr = HPA2HVA(hpa);
		memcpy_s(g_ptr, len, h_ptr, len);
		gpa += len;
		h_ptr += len;
		size -= len;
	} while (size > 0);

	return 0;
}

enum vm_paging_mode get_vcpu_paging_mode(struct vcpu *vcpu)
{
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	enum vm_cpu_mode cpu_mode;

	cpu_mode = get_vcpu_mode(vcpu);

	if (cpu_mode == CPU_MODE_REAL)
		return PAGING_MODE_0_LEVEL;
	else if (cpu_mode == CPU_MODE_PROTECTED) {
		if (cur_context->cr4 & CR4_PAE)
			return PAGING_MODE_3_LEVEL;
		else if (cur_context->cr0 & CR0_PG)
			return PAGING_MODE_2_LEVEL;
		return PAGING_MODE_0_LEVEL;
	} else	/* compatibility or 64bit mode */
		return PAGING_MODE_4_LEVEL;
}

/* TODO: Add code to check for Revserved bits, SMAP and PKE when do translation
 * during page walk */
static int _gva2gpa_common(struct vcpu *vcpu, struct page_walk_info *pw_info,
	uint64_t gva, uint64_t *gpa, uint32_t *err_code)
{
	int i, index, shift;
	uint8_t *base;
	uint64_t entry;
	uint64_t addr, page_size;
	int ret = 0;
	int fault = 0;

	if (pw_info->level < 1)
		return -EINVAL;

	addr = pw_info->top_entry;
	for (i = pw_info->level - 1; i >= 0; i--) {
		addr = addr & IA32E_REF_MASK;
		base = GPA2HVA(vcpu->vm, addr);
		if (base == NULL) {
			ret = -EFAULT;
			goto out;
		}

		shift = i * pw_info->width + 12;
		index = (gva >> shift) & ((1UL << pw_info->width) - 1);
		page_size = 1UL << shift;

		if (pw_info->width == 10)
			/* 32bit entry */
			entry = *((uint32_t *)(base + 4 * index));
		else
			entry = *((uint64_t *)(base + 8 * index));

		/* check if the entry present */
		if (!(entry & MMU_32BIT_PDE_P)) {
			ret = -EFAULT;
			goto out;
		}
		/* check for R/W */
		if (pw_info->is_write_access && !(entry & MMU_32BIT_PDE_RW)) {
			/* Case1: Supermode and wp is 1
			 * Case2: Usermode */
			if (!(!pw_info->is_user_mode && !pw_info->wp))
				fault = 1;
		}
		/* check for nx, since for 32-bit paing, the XD bit is
		 * reserved(0), use the same logic as PAE/4-level paging */
		if (pw_info->is_inst_fetch && pw_info->nxe &&
		    (entry & MMU_MEM_ATTR_BIT_EXECUTE_DISABLE))
			fault = 1;

		/* check for U/S */
		if (!(entry & MMU_32BIT_PDE_US) && pw_info->is_user_mode)
			fault = 1;

		if (pw_info->pse && (i > 0 && (entry & MMU_32BIT_PDE_PS)))
			break;
		addr = entry;
	}

	entry >>= shift;
	/* shift left 12bit more and back to clear XD/Prot Key/Ignored bits */
	entry <<= (shift + 12);
	entry >>= 12;
	*gpa = entry | (gva & (page_size - 1));
out:

	if (fault) {
		ret = -EFAULT;
		*err_code |= PAGE_FAULT_P_FLAG;
	}
	return ret;
}

static int _gva2gpa_pae(struct vcpu *vcpu, struct page_walk_info *pw_info,
	uint64_t gva, uint64_t *gpa, uint32_t *err_code)
{
	int index;
	uint64_t *base;
	uint64_t entry;
	uint64_t addr;
	int ret;

	addr = pw_info->top_entry & 0xFFFFFFF0UL;
	base = GPA2HVA(vcpu->vm, addr);
	if (base == NULL) {
		ret = -EFAULT;
		goto out;
	}

	index = (gva >> 30) & 0x3;
	entry = base[index];

	if (!(entry & MMU_32BIT_PDE_P)) {
		ret = -EFAULT;
		goto out;
	}

	pw_info->level = 2;
	pw_info->top_entry = entry;
	ret = _gva2gpa_common(vcpu, pw_info, gva, gpa, err_code);

out:
	return ret;

}

/* Refer to SDM Vol.3A 6-39 section 6.15 for the format of paging fault error
 * code.
 *
 * Caller should set the contect of err_code properly according to the address
 * usage when calling this function:
 * - If it is an address for write, set PAGE_FAULT_WR_FLAG in err_code.
 * - If it is an address for instruction featch, set PAGE_FAULT_ID_FLAG in
 *   err_code.
 * Caller should check the return value to confirm if the function success or
 * not.
 * If a protection volation detected during page walk, this function still will
 * give the gpa translated, it is up to caller to decide if it need to inject a
 * #PF or not.
 * - Return 0 for success.
 * - Return -EINVAL for invalid parameter.
 * - Return -EFAULT for paging fault, and refer to err_code for paging fault
 *   error code.
 */
int gva2gpa(struct vcpu *vcpu, uint64_t gva, uint64_t *gpa,
	uint32_t *err_code)
{
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	enum vm_paging_mode pm = get_vcpu_paging_mode(vcpu);
	struct page_walk_info pw_info;
	int ret = 0;

	if (!gpa || !err_code)
		return -EINVAL;
	*gpa = 0;

	pw_info.top_entry = cur_context->cr3;
	pw_info.level = pm;
	pw_info.is_write_access = !!(*err_code & PAGE_FAULT_WR_FLAG);
	pw_info.is_inst_fetch = !!(*err_code & PAGE_FAULT_ID_FLAG);
	pw_info.is_user_mode = ((exec_vmread(VMX_GUEST_CS_SEL) & 0x3) == 3);
	pw_info.pse = true;
	pw_info.nxe = cur_context->ia32_efer & MSR_IA32_EFER_NXE_BIT;
	pw_info.wp = !!(cur_context->cr0 & CR0_WP);

	*err_code &=  ~PAGE_FAULT_P_FLAG;

	if (pm == PAGING_MODE_4_LEVEL) {
		pw_info.width = 9;
		ret = _gva2gpa_common(vcpu, &pw_info, gva, gpa, err_code);
	} else if(pm == PAGING_MODE_3_LEVEL) {
		pw_info.width = 9;
		ret = _gva2gpa_pae(vcpu, &pw_info, gva, gpa, err_code);
	} else if (pm == PAGING_MODE_2_LEVEL) {
		pw_info.width = 10;
		pw_info.pse = !!(cur_context->cr4 & CR4_PSE);
		pw_info.nxe = false;
		ret = _gva2gpa_common(vcpu, &pw_info, gva, gpa, err_code);
	} else
		*gpa = gva;

	if (ret == -EFAULT) {
		if (pw_info.is_user_mode)
			*err_code |= PAGE_FAULT_US_FLAG;
	}

	return ret;
}


void init_e820(void)
{
	unsigned int i;

	if (boot_regs[0] == MULTIBOOT_INFO_MAGIC) {
		struct multiboot_info *mbi =
			(struct multiboot_info *)((uint64_t)boot_regs[1]);
		pr_info("Multiboot info detected\n");
		if (mbi->mi_flags & 0x40) {
			struct multiboot_mmap *mmap =
				(struct multiboot_mmap *)
				((uint64_t)mbi->mi_mmap_addr);
			e820_entries = mbi->mi_mmap_length/
				sizeof(struct multiboot_mmap);
			if (e820_entries > E820_MAX_ENTRIES) {
				pr_err("Too many E820 entries %d\n",
					e820_entries);
				e820_entries = E820_MAX_ENTRIES;
			}
			dev_dbg(ACRN_DBG_GUEST,
				"mmap length 0x%x addr 0x%x entries %d\n",
				mbi->mi_mmap_length, mbi->mi_mmap_addr,
				e820_entries);
			for (i = 0; i < e820_entries; i++) {
				e820[i].baseaddr = mmap[i].baseaddr;
				e820[i].length = mmap[i].length;
				e820[i].type = mmap[i].type;

				dev_dbg(ACRN_DBG_GUEST,
					"mmap table: %d type: 0x%x\n",
					i, mmap[i].type);
				dev_dbg(ACRN_DBG_GUEST,
					"Base: 0x%016llx length: 0x%016llx",
					mmap[i].baseaddr, mmap[i].length);
			}
		}
	} else
		ASSERT(0, "no multiboot info found");
}


void obtain_e820_mem_info(void)
{
	unsigned int i;
	struct e820_entry *entry;

	e820_mem.mem_bottom = UINT64_MAX;
	e820_mem.mem_top = 0x00;
	e820_mem.total_mem_size = 0;
	e820_mem.max_ram_blk_base = 0;
	e820_mem.max_ram_blk_size = 0;

	for (i = 0; i < e820_entries; i++) {
		entry = &e820[i];
		if (e820_mem.mem_bottom > entry->baseaddr)
			e820_mem.mem_bottom = entry->baseaddr;

		if (entry->baseaddr + entry->length
				> e820_mem.mem_top) {
			e820_mem.mem_top = entry->baseaddr
				+ entry->length;
		}

		if (entry->type == E820_TYPE_RAM) {
			e820_mem.total_mem_size += entry->length;
			if (entry->baseaddr == UOS_DEFAULT_START_ADDR) {
				e820_mem.max_ram_blk_base =
					entry->baseaddr;
				e820_mem.max_ram_blk_size = entry->length;
			}
		}
	}
}

static void rebuild_vm0_e820(void)
{
	unsigned int i;
	uint64_t entry_start;
	uint64_t entry_end;
	uint64_t hv_start = CONFIG_RAM_START;
	uint64_t hv_end  = hv_start + CONFIG_RAM_SIZE;
	struct e820_entry *entry, new_entry = {0};

	/* hypervisor mem need be filter out from e820 table
	 * it's hv itself + other hv reserved mem like vgt etc
	 */
	for (i = 0; i < e820_entries; i++) {
		entry = &e820[i];
		entry_start = entry->baseaddr;
		entry_end = entry->baseaddr + entry->length;

		/* No need handle in these cases*/
		if (entry->type != E820_TYPE_RAM || entry_end <= hv_start
				|| entry_start >= hv_end) {
			continue;
		}

		/* filter out hv mem and adjust length of this entry*/
		if (entry_start < hv_start && entry_end <= hv_end) {
			entry->length = hv_start - entry_start;
			continue;
		}
		/* filter out hv mem and need to create a new entry*/
		if (entry_start < hv_start && entry_end > hv_end) {
			entry->length = hv_start - entry_start;
			new_entry.baseaddr = hv_end;
			new_entry.length = entry_end - hv_end;
			new_entry.type = E820_TYPE_RAM;
			continue;
		}
		/* This entry is within the range of hv mem
		 * change to E820_TYPE_RESERVED
		 */
		if (entry_start >= hv_start && entry_end <= hv_end) {
			entry->type = E820_TYPE_RESERVED;
			continue;
		}

		if (entry_start >= hv_start && entry_start < hv_end
				&& entry_end > hv_end) {
			entry->baseaddr = hv_end;
			entry->length = entry_end - hv_end;
			continue;
		}

	}

	if (new_entry.length > 0) {
		e820_entries++;
		ASSERT(e820_entries <= E820_MAX_ENTRIES,
				"e820 entry overflow");
		entry = &e820[e820_entries - 1];
		entry->baseaddr = new_entry.baseaddr;
		entry->length = new_entry.length;
		entry->type = new_entry.type;
	}

	e820_mem.total_mem_size -= CONFIG_RAM_SIZE;
}

int prepare_vm0_memmap_and_e820(struct vm *vm)
{
	unsigned int i;
	uint32_t attr_wb = (MMU_MEM_ATTR_READ |
			MMU_MEM_ATTR_WRITE   |
			MMU_MEM_ATTR_EXECUTE |
			MMU_MEM_ATTR_WB_CACHE);
	uint32_t attr_uc = (MMU_MEM_ATTR_READ |
			MMU_MEM_ATTR_WRITE   |
			MMU_MEM_ATTR_EXECUTE |
			MMU_MEM_ATTR_UNCACHED);
	struct e820_entry *entry;


	ASSERT(is_vm0(vm), "This func only for vm0");

	rebuild_vm0_e820();
	dev_dbg(ACRN_DBG_GUEST,
		"vm0: bottom memory - 0x%llx, top memory - 0x%llx\n",
		e820_mem.mem_bottom, e820_mem.mem_top);

	/* create real ept map for all ranges with UC */
	ept_mmap(vm, e820_mem.mem_bottom, e820_mem.mem_bottom,
			(e820_mem.mem_top - e820_mem.mem_bottom),
			MAP_MMIO, attr_uc);

	/* update ram entries to WB attr */
	for (i = 0; i < e820_entries; i++) {
		entry = &e820[i];
		if (entry->type == E820_TYPE_RAM)
			ept_mmap(vm, entry->baseaddr, entry->baseaddr,
					entry->length, MAP_MEM, attr_wb);
	}


	dev_dbg(ACRN_DBG_GUEST, "VM0 e820 layout:\n");
	for (i = 0; i < e820_entries; i++) {
		entry = &e820[i];
		dev_dbg(ACRN_DBG_GUEST,
			"e820 table: %d type: 0x%x", i, entry->type);
		dev_dbg(ACRN_DBG_GUEST,
			"BaseAddress: 0x%016llx length: 0x%016llx\n",
			entry->baseaddr, entry->length);
	}

	/* unmap hypervisor itself for safety
	 * will cause EPT violation if sos accesses hv memory
	 */
	ept_mmap(vm, CONFIG_RAM_START, CONFIG_RAM_START,
			CONFIG_RAM_SIZE, MAP_UNMAP, 0);
	return 0;
}

#ifdef CONFIG_START_VM0_BSP_64BIT
/*******************************************************************
 *         GUEST initial page table
 *
 * guest starts with long mode, HV needs to prepare Guest identity
 * mapped page table.
 * For SOS:
 *   Guest page tables cover 0~4G space with 2M page size, will use
 *   6 pages memory for page tables.
 * For UOS(Trusty not enabled):
 *   Guest page tables cover 0~4G space with 2M page size, will use
 *    6 pages memory for page tables.
 * For UOS(Trusty enabled):
 *   Guest page tables cover 0~4G and trusy memory space with 2M page size,
 *   will use 7 pages memory for page tables.
 * This API assume that the trusty memory is remapped to guest physical address
 * of 511G to 511G + 16MB
 *
 * FIXME: here using hard code GUEST_INIT_PAGE_TABLE_START as guest init page
 * table gpa start, and it will occupy at most GUEST_INIT_PT_PAGE_NUM pages.
 * Some check here:
 * - guest page table space should not override cpu_secondary_reset code area
 *   (it's a little tricky here, as under current identical mapping, HV & SOS
 *   share same memory under 1M; under uefi boot mode, the defered AP startup
 *   need cpu_secondary_reset code area which reserved by uefi stub keep there
 *   no change even after SOS startup)
 * - guest page table space should not override possible RSDP fix segment
 *
 * Anyway, it's a tmp solution, the init page tables should be totally removed
 * after guest realmode/32bit no paging mode got supported.
 ******************************************************************/
#define GUEST_INIT_PAGE_TABLE_SKIP_SIZE	0x8000UL
#define GUEST_INIT_PAGE_TABLE_START	(CONFIG_LOW_RAM_START +	\
					GUEST_INIT_PAGE_TABLE_SKIP_SIZE)
#define GUEST_INIT_PT_PAGE_NUM		7
#define RSDP_F_ADDR			0xE0000
uint64_t create_guest_initial_paging(struct vm *vm)
{
	uint64_t i = 0;
	uint64_t entry = 0;
	uint64_t entry_num = 0;
	uint64_t pdpt_base_paddr = 0;
	uint64_t pd_base_paddr = 0;
	uint64_t table_present = 0;
	uint64_t table_offset = 0;
	void *addr = NULL;
	void *pml4_addr = GPA2HVA(vm, GUEST_INIT_PAGE_TABLE_START);

	ASSERT((GUEST_INIT_PAGE_TABLE_START + 7 * PAGE_SIZE_4K) <
		RSDP_F_ADDR, "RSDP fix segment could be override");

	if (GUEST_INIT_PAGE_TABLE_SKIP_SIZE <
		(unsigned long)&_ld_cpu_secondary_reset_size) {
		panic("guest init PTs override cpu_secondary_reset code");
	}

	/* Using continuous memory for guest page tables, the total 4K page
	 * number for it(without trusty) is GUEST_INIT_PT_PAGE_NUM-1.
	 * here make sure they are init as 0 (page entry no present)
	 */
	memset(pml4_addr, 0, PAGE_SIZE_4K * GUEST_INIT_PT_PAGE_NUM-1);

	/* Write PML4E */
	table_present = (IA32E_COMM_P_BIT | IA32E_COMM_RW_BIT);
	/* PML4 used 1 page, skip it to fetch PDPT */
	pdpt_base_paddr = GUEST_INIT_PAGE_TABLE_START + PAGE_SIZE_4K;
	entry = pdpt_base_paddr | table_present;
	MEM_WRITE64(pml4_addr, entry);

	/* Write PDPTE, PDPT used 1 page, skip it to fetch PD */
	pd_base_paddr = pdpt_base_paddr + PAGE_SIZE_4K;
	addr = pml4_addr + PAGE_SIZE_4K;
	/* Guest page tables cover 0~4G space with 2M page size */
	for (i = 0; i < 4; i++) {
		entry = ((pd_base_paddr + (i * PAGE_SIZE_4K))
				| table_present);
		MEM_WRITE64(addr, entry);
		addr += IA32E_COMM_ENTRY_SIZE;
	}

	/* Write PDE, PT used 4 pages */
	table_present = (IA32E_PDPTE_PS_BIT
			| IA32E_COMM_P_BIT
			| IA32E_COMM_RW_BIT);
	/* Totally 2048(512*4) entries with 2M page size for 0~4G*/
	entry_num = IA32E_NUM_ENTRIES * 4;
	addr = pml4_addr + 2 * PAGE_SIZE_4K;
	for (i = 0; i < entry_num; i++) {
		entry = (i * (1 << MMU_PDE_PAGE_SHIFT)) | table_present;
		MEM_WRITE64(addr, entry);
		addr += IA32E_COMM_ENTRY_SIZE;
	}

	/* For UOS, if trusty is enabled,
	 * need to setup tempory page table for trusty
	 * FIXME: this is a tempory solution for trusty enabling,
	 * the final solution is that vSBL will setup guest page tables
	 */
	if (vm->sworld_control.sworld_enabled && !is_vm0(vm)) {
		/* clear page entry for trusty */
		memset(pml4_addr + 6 * PAGE_SIZE_4K, 0, PAGE_SIZE_4K);

		/* Write PDPTE for trusy memory, PD will use 7th page */
		pd_base_paddr = GUEST_INIT_PAGE_TABLE_START +
				(6 * PAGE_SIZE_4K);
		table_offset =
			IA32E_PDPTE_INDEX_CALC(TRUSTY_EPT_REBASE_GPA);
		addr = (pml4_addr + PAGE_SIZE_4K + table_offset);
		table_present = (IA32E_COMM_P_BIT | IA32E_COMM_RW_BIT);
		entry = (pd_base_paddr | table_present);
		MEM_WRITE64(addr, entry);

		/* Write PDE for trusty with 2M page size */
		entry_num = TRUSTY_MEMORY_SIZE / (1 << MMU_PDE_PAGE_SHIFT);
		addr = pml4_addr + 6 * PAGE_SIZE_4K;
		table_present = (IA32E_PDPTE_PS_BIT
				| IA32E_COMM_P_BIT
				| IA32E_COMM_RW_BIT);
		for (i = 0; i < entry_num; i++) {
			entry = (TRUSTY_EPT_REBASE_GPA +
				(i * (1 << MMU_PDE_PAGE_SHIFT)))
				| table_present;
			MEM_WRITE64(addr, entry);
			addr += IA32E_COMM_ENTRY_SIZE;
		}
	}

	return GUEST_INIT_PAGE_TABLE_START;
}
#endif

/*******************************************************************
 *         GUEST initial GDT table
 *
 * If guest starts with protected mode, HV needs to prepare Guest GDT.
 ******************************************************************/

#define GUEST_INIT_GDT_SKIP_SIZE	0x8000UL
#define GUEST_INIT_GDT_START	(CONFIG_LOW_RAM_START +	\
					GUEST_INIT_GDT_SKIP_SIZE)

/* The GDT defined below compatible with linux kernel */
#define GUEST_INIT_GDT_DESC_0	(0x0)
#define GUEST_INIT_GDT_DESC_1	(0x0)
#define GUEST_INIT_GDT_DESC_2	(0x00CF9B000000FFFFULL) /* Linear Code */
#define GUEST_INIT_GDT_DESC_3	(0x00CF93000000FFFFULL) /* Linear Data */

static const uint64_t guest_init_gdt[] = {
	GUEST_INIT_GDT_DESC_0,
	GUEST_INIT_GDT_DESC_1,
	GUEST_INIT_GDT_DESC_2,
	GUEST_INIT_GDT_DESC_3,
};

uint32_t create_guest_init_gdt(struct vm *vm, uint32_t *limit)
{
	void *gtd_addr = GPA2HVA(vm, GUEST_INIT_GDT_START);

	*limit = sizeof(guest_init_gdt) - 1;
	memcpy_s(gtd_addr, 64, guest_init_gdt, sizeof(guest_init_gdt));

	return GUEST_INIT_GDT_START;
};
