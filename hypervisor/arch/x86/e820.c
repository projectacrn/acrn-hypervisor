/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <acrn_hv_defs.h>
#include <page.h>
#include <e820.h>
#include <mmu.h>
#include <multiboot.h>
#include <logmsg.h>

/*
 * e820.c contains the related e820 operations; like HV to get memory info for its MMU setup;
 * and hide HV memory from SOS_VM...
 */

static uint32_t hv_e820_entries_nr;
/* Describe the memory layout the hypervisor uses */
static struct e820_entry hv_e820[E820_MAX_ENTRIES];
/* Describe the top/bottom/size of the physical memory the hypervisor manages */
static struct mem_range hv_mem_range;

#define ACRN_DBG_E820	6U

static void obtain_mem_range_info(void)
{
	uint32_t i;
	struct e820_entry *entry;

	hv_mem_range.mem_bottom = UINT64_MAX;
	hv_mem_range.mem_top = 0x0UL;
	hv_mem_range.total_mem_size = 0UL;

	for (i = 0U; i < hv_e820_entries_nr; i++) {
		entry = &hv_e820[i];

		if (hv_mem_range.mem_bottom > entry->baseaddr) {
			hv_mem_range.mem_bottom = entry->baseaddr;
		}

		if ((entry->baseaddr + entry->length) > hv_mem_range.mem_top) {
			hv_mem_range.mem_top = entry->baseaddr + entry->length;
		}

		if (entry->type == E820_TYPE_RAM) {
			hv_mem_range.total_mem_size += entry->length;
		}
	}
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

	for (i = 0U; i < hv_e820_entries_nr; i++) {
		entry = &hv_e820[i];
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
			hv_mem_range.total_mem_size -= size;
			ret = start;
			break;
		}

		/*
		 * found entry with available memory larger than requested
		 * allocate memory from the end of this entry at page boundary
		 */
		new_entry = &hv_e820[hv_e820_entries_nr];
		new_entry->type = E820_TYPE_RESERVED;
		new_entry->baseaddr = end - size;
		new_entry->length = (entry->baseaddr + entry->length) - new_entry->baseaddr;

		/* Shrink the existing entry and total available memory */
		entry->length -= new_entry->length;
		hv_mem_range.total_mem_size -= new_entry->length;
		hv_e820_entries_nr++;

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
	uint64_t top_addr_space = CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE;

	if (boot_regs[0] == MULTIBOOT_INFO_MAGIC) {
		/*
		 * Before installing new PML4 table in enable_paging(), HPA->HVA is always 1:1 mapping
		 * and hpa2hva() can't be used to do the conversion. Here we simply treat boot_reg[1] as HPA.
		 */
		uint64_t hpa = (uint64_t)boot_regs[1];
		struct multiboot_info *mbi = (struct multiboot_info *)hpa;

		pr_info("Multiboot info detected\n");
		if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_MMAP) != 0U) {
			/* HPA->HVA is always 1:1 mapping at this moment */
			hpa = (uint64_t)mbi->mi_mmap_addr;
			struct multiboot_mmap *mmap = (struct multiboot_mmap *)hpa;

			hv_e820_entries_nr = mbi->mi_mmap_length / sizeof(struct multiboot_mmap);
			if (hv_e820_entries_nr > E820_MAX_ENTRIES) {
				pr_err("Too many E820 entries %d\n", hv_e820_entries_nr);
				hv_e820_entries_nr = E820_MAX_ENTRIES;
			}

			dev_dbg(ACRN_DBG_E820, "mmap length 0x%x addr 0x%x entries %d\n",
				mbi->mi_mmap_length, mbi->mi_mmap_addr, hv_e820_entries_nr);

			for (i = 0U; i < hv_e820_entries_nr; i++) {
				if (mmap[i].baseaddr >= top_addr_space) {
					mmap[i].length = 0UL;
				} else {
					if ((mmap[i].baseaddr + mmap[i].length) > top_addr_space) {
						mmap[i].length = top_addr_space - mmap[i].baseaddr;
					}
				}

				hv_e820[i].baseaddr = mmap[i].baseaddr;
				hv_e820[i].length = mmap[i].length;
				hv_e820[i].type = mmap[i].type;

				dev_dbg(ACRN_DBG_E820, "mmap table: %d type: 0x%x\n", i, mmap[i].type);
				dev_dbg(ACRN_DBG_E820, "Base: 0x%016llx length: 0x%016llx",
					mmap[i].baseaddr, mmap[i].length);
			}
		}

		obtain_mem_range_info();
	} else {
		panic("no multiboot info found");
	}
}

uint32_t get_e820_entries_count(void)
{
	return hv_e820_entries_nr;
}

const struct e820_entry *get_e820_entry(void)
{
	return hv_e820;
}

const struct mem_range *get_mem_range_info(void)
{
	return &hv_mem_range;
}
