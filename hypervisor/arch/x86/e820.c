/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <reloc.h>
#include <multiboot.h>
#include <e820.h>

/*
 * e820.c contains the related e820 operations; like HV to get memory info for its MMU setup;
 * and hide HV memory from SOS_VM...
 */

static uint32_t e820_entries_count;
static struct e820_entry e820[E820_MAX_ENTRIES];
static struct e820_mem_params e820_mem;

#define ACRN_DBG_E820	6U

static void obtain_e820_mem_info(void)
{
	uint32_t i;
	struct e820_entry *entry;

	e820_mem.mem_bottom = UINT64_MAX;
	e820_mem.mem_top = 0x0UL;
	e820_mem.total_mem_size = 0UL;
	e820_mem.max_ram_blk_base = 0UL;
	e820_mem.max_ram_blk_size = 0UL;

	for (i = 0U; i < e820_entries_count; i++) {
		entry = &e820[i];
		if (e820_mem.mem_bottom > entry->baseaddr) {
			e820_mem.mem_bottom = entry->baseaddr;
		}

		if ((entry->baseaddr + entry->length) > e820_mem.mem_top) {
			e820_mem.mem_top = entry->baseaddr + entry->length;
		}

		if (entry->type == E820_TYPE_RAM) {
			e820_mem.total_mem_size += entry->length;
			if (entry->baseaddr == UOS_DEFAULT_START_ADDR) {
				e820_mem.max_ram_blk_base = entry->baseaddr;
				e820_mem.max_ram_blk_size = entry->length;
			}
		}
	}
}

/* before boot sos_vm(service OS), call it to hide the HV RAM entry in e820 table from sos_vm */
void rebuild_sos_vm_e820(void)
{
	uint32_t i;
	uint64_t entry_start;
	uint64_t entry_end;
	uint64_t hv_start_pa = get_hv_image_base();
	uint64_t hv_end_pa  = hv_start_pa + CONFIG_HV_RAM_SIZE;
	struct e820_entry *entry, new_entry = {0};

	/* hypervisor mem need be filter out from e820 table
	 * it's hv itself + other hv reserved mem like vgt etc
	 */
	for (i = 0U; i < e820_entries_count; i++) {
		entry = &e820[i];
		entry_start = entry->baseaddr;
		entry_end = entry->baseaddr + entry->length;

		/* No need handle in these cases*/
		if ((entry->type != E820_TYPE_RAM) || (entry_end <= hv_start_pa) || (entry_start >= hv_end_pa)) {
			continue;
		}

		/* filter out hv mem and adjust length of this entry*/
		if ((entry_start < hv_start_pa) && (entry_end <= hv_end_pa)) {
			entry->length = hv_start_pa - entry_start;
			continue;
		}

		/* filter out hv mem and need to create a new entry*/
		if ((entry_start < hv_start_pa) && (entry_end > hv_end_pa)) {
			entry->length = hv_start_pa - entry_start;
			new_entry.baseaddr = hv_end_pa;
			new_entry.length = entry_end - hv_end_pa;
			new_entry.type = E820_TYPE_RAM;
			continue;
		}

		/* This entry is within the range of hv mem
		 * change to E820_TYPE_RESERVED
		 */
		if ((entry_start >= hv_start_pa) && (entry_end <= hv_end_pa)) {
			entry->type = E820_TYPE_RESERVED;
			continue;
		}

		if ((entry_start >= hv_start_pa) && (entry_start < hv_end_pa) && (entry_end > hv_end_pa)) {
			entry->baseaddr = hv_end_pa;
			entry->length = entry_end - hv_end_pa;
			continue;
		}
	}

	if (new_entry.length > 0UL) {
		e820_entries_count++;
		ASSERT(e820_entries_count <= E820_MAX_ENTRIES, "e820 entry overflow");
		entry = &e820[e820_entries_count - 1];
		entry->baseaddr = new_entry.baseaddr;
		entry->length = new_entry.length;
		entry->type = new_entry.type;
	}

	e820_mem.total_mem_size -= CONFIG_HV_RAM_SIZE;
}

/* get some RAM below 1MB in e820 entries, hide it from sos_vm, return its start address */
uint64_t e820_alloc_low_memory(uint32_t size_arg)
{
	uint32_t i;
	uint32_t size = size_arg;
	uint64_t ret = ACRN_INVALID_HPA;
	struct e820_entry *entry, *new_entry;

	/* We want memory in page boundary and integral multiple of pages */
	size = (((size + PAGE_SIZE) - 1U) >> PAGE_SHIFT) << PAGE_SHIFT;

	for (i = 0U; i < e820_entries_count; i++) {
		entry = &e820[i];
		uint64_t start, end, length;

		start = round_page_up(entry->baseaddr);
		end = round_page_down(entry->baseaddr + entry->length);
		length = end - start;
		length = (end > start) ? (end - start) : 0;

		/* Search for available low memory */
		if ((entry->type != E820_TYPE_RAM) || (length < size) || ((start + size) > MEM_1M)) {
			continue;
		}

		/* found exact size of e820 entry */
		if (length == size) {
			entry->type = E820_TYPE_RESERVED;
			e820_mem.total_mem_size -= size;
			ret = start;
			break;
		}

		/*
		 * found entry with available memory larger than requested
		 * allocate memory from the end of this entry at page boundary
		 */
		new_entry = &e820[e820_entries_count];
		new_entry->type = E820_TYPE_RESERVED;
		new_entry->baseaddr = end - size;
		new_entry->length = (entry->baseaddr + entry->length) - new_entry->baseaddr;

		/* Shrink the existing entry and total available memory */
		entry->length -= new_entry->length;
		e820_mem.total_mem_size -= new_entry->length;
		e820_entries_count++;

	        ret = new_entry->baseaddr;
		break;
	}

	if (ret == ACRN_INVALID_HPA) {
		pr_fatal("Can't allocate memory under 1M from E820\n");
	}
	return ret;
}

/* HV read multiboot header to get e820 entries info and calc total RAM info */
void init_e820(void)
{
	uint32_t i;

	if (boot_regs[0] == MULTIBOOT_INFO_MAGIC) {
		struct multiboot_info *mbi = (struct multiboot_info *)(hpa2hva((uint64_t)boot_regs[1]));

		pr_info("Multiboot info detected\n");
		if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_MMAP) != 0U) {
			struct multiboot_mmap *mmap = (struct multiboot_mmap *)hpa2hva((uint64_t)mbi->mi_mmap_addr);

			e820_entries_count = mbi->mi_mmap_length / sizeof(struct multiboot_mmap);
			if (e820_entries_count > E820_MAX_ENTRIES) {
				pr_err("Too many E820 entries %d\n", e820_entries_count);
				e820_entries_count = E820_MAX_ENTRIES;
			}

			dev_dbg(ACRN_DBG_E820, "mmap length 0x%x addr 0x%x entries %d\n",
				mbi->mi_mmap_length, mbi->mi_mmap_addr, e820_entries_count);

			for (i = 0U; i < e820_entries_count; i++) {
				e820[i].baseaddr = mmap[i].baseaddr;
				e820[i].length = mmap[i].length;
				e820[i].type = mmap[i].type;

				dev_dbg(ACRN_DBG_E820, "mmap table: %d type: 0x%x\n", i, mmap[i].type);
				dev_dbg(ACRN_DBG_E820, "Base: 0x%016llx length: 0x%016llx",
					mmap[i].baseaddr, mmap[i].length);
			}
		}

		obtain_e820_mem_info();
	} else {
		panic("no multiboot info found");
	}
}

uint32_t get_e820_entries_count(void)
{
	return e820_entries_count;
}

const struct e820_entry *get_e820_entry(void)
{
	return e820;
}

const struct e820_mem_params *get_e820_mem_info(void)
{
	return &e820_mem;
}

#ifdef CONFIG_PARTITION_MODE
uint32_t create_e820_table(struct e820_entry *param_e820)
{
	uint32_t i;

	for (i = 0U; i < NUM_E820_ENTRIES; i++) {
		param_e820[i].baseaddr = ve820_entry[i].baseaddr;
		param_e820[i].length = ve820_entry[i].length;
		param_e820[i].type = ve820_entry[i].type;
	}

	return NUM_E820_ENTRIES;
}
#else
uint32_t create_e820_table(struct e820_entry *param_e820)
{
	uint32_t i;

	ASSERT(e820_entries_count > 0U, "e820 should be inited");

	for (i = 0U; i < e820_entries_count; i++) {
		param_e820[i].baseaddr = e820[i].baseaddr;
		param_e820[i].length = e820[i].length;
		param_e820[i].type = e820[i].type;
	}

	return e820_entries_count;
}
#endif
