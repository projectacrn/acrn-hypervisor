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

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <bsp_cfg.h>
#include <bsp_extern.h>
#include <acrn_hv_defs.h>
#include <hv_debug.h>
#include <multiboot.h>
#include <zeropage.h>

#define ACRN_DBG_GUEST	6

/* for VM0 e820 */
uint32_t e820_entries;
struct e820_entry e820[E820_MAX_ENTRIES];
struct e820_mem_params e820_mem;

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

	while ((vcpu_id = bitmap_ffs(&vdmask)) >= 0) {
		bitmap_clr(vcpu_id, &vdmask);
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
		size -= len;
	} while (size > 0);

	return 0;
}

uint64_t gva2gpa(struct vm *vm, uint64_t cr3, uint64_t gva)
{
	int level, index, shift;
	uint64_t *base, addr, entry, page_size;
	uint64_t gpa = 0;

	addr = cr3;

	for (level = 3; level >= 0; level--) {
		addr = addr & IA32E_REF_MASK;
		base = GPA2HVA(vm, addr);
		ASSERT(base != NULL, "invalid ptp base.");
		shift = level * 9 + 12;
		index = (gva >> shift) & 0x1FF;
		page_size = 1UL << shift;

		entry = base[index];
		if (level > 0 && (entry & MMU_32BIT_PDE_PS) != 0)
			break;
		addr = entry;
	}

	entry >>= shift; entry <<= (shift + 12); entry >>= 12;
	gpa = entry | (gva & (page_size - 1));

	return gpa;
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

	_Static_assert((GUEST_INIT_PAGE_TABLE_START + 7 * PAGE_SIZE_4K) <
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
