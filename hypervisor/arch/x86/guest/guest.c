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

#define BOOT_ARGS_LOAD_ADDR				0x24EFC000

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

int init_vm0_boot_info(struct vm *vm)
{
	struct multiboot_module *mods = NULL;
	struct multiboot_info *mbi = NULL;

	if (!is_vm0(vm)) {
		pr_err("just for vm0 to get info!");
		return -EINVAL;
	}

	if (boot_regs[0] != MULTIBOOT_INFO_MAGIC) {
		ASSERT(0, "no multiboot info found");
		return -EINVAL;
	}

	mbi = (struct multiboot_info *)((uint64_t)boot_regs[1]);

	dev_dbg(ACRN_DBG_GUEST, "Multiboot detected, flag=0x%x", mbi->mi_flags);
	if (!(mbi->mi_flags & MULTIBOOT_INFO_HAS_MODS)) {
		ASSERT(0, "no sos kernel info found");
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_GUEST, "mod counts=%d\n", mbi->mi_mods_count);

	/* mod[0] is for kernel&cmdline, other mod for ramdisk/firmware info*/
	mods = (struct multiboot_module *)(uint64_t)mbi->mi_mods_addr;

	dev_dbg(ACRN_DBG_GUEST, "mod0 start=0x%x, end=0x%x",
		mods[0].mm_mod_start, mods[0].mm_mod_end);
	dev_dbg(ACRN_DBG_GUEST, "cmd addr=0x%x, str=%s", mods[0].mm_string,
		(char *) (uint64_t)mods[0].mm_string);

	vm->sw.kernel_type = VM_LINUX_GUEST;
	vm->sw.kernel_info.kernel_src_addr =
		(void *)(uint64_t)mods[0].mm_mod_start;
	vm->sw.kernel_info.kernel_size =
		mods[0].mm_mod_end - mods[0].mm_mod_start;
	vm->sw.kernel_info.kernel_load_addr =
		(void *)(uint64_t)mods[0].mm_mod_start;

	vm->sw.linux_info.bootargs_src_addr =
		(void *)(uint64_t)mods[0].mm_string;
	vm->sw.linux_info.bootargs_load_addr =
		(void *)BOOT_ARGS_LOAD_ADDR;
	vm->sw.linux_info.bootargs_size =
		strnlen_s((char *)(uint64_t) mods[0].mm_string, MEM_2K);

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

		if (entry->baseaddr == UOS_DEFAULT_START_ADDR
				&& entry->type == E820_TYPE_RAM) {
			e820_mem.max_ram_blk_base =
				entry->baseaddr;
			e820_mem.max_ram_blk_size = entry->length;
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
