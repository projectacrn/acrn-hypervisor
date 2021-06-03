/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <acrn_hv_defs.h>
#include <asm/page.h>
#include <asm/e820.h>
#include <asm/mmu.h>
#include <boot.h>
#include <logmsg.h>
#include <asm/guest/ept.h>

/*
 * e820.c contains the related e820 operations; like HV to get memory info for its MMU setup;
 * and hide HV memory from SOS_VM...
 */

static uint32_t hv_e820_entries_nr;
/* Describe the memory layout the hypervisor uses */
static struct e820_entry hv_e820[E820_MAX_ENTRIES];

#define DBG_LEVEL_E820	6U

/*
 * @brief reserve some RAM, hide it from sos_vm, return its start address
 * @param size_arg Amount of memory to be found and marked reserved
 * @param max_addr Maximum address below which memory is to be identified
 *
 * @pre hv_e820_entries_nr > 0U
 * @pre (size_arg & 0xFFFU) == 0U
 * @return base address of the memory region
 */
uint64_t e820_alloc_memory(uint32_t size_arg, uint64_t max_addr)
{
	int32_t i;
	uint64_t size = size_arg;
	uint64_t ret = INVALID_HPA;
	struct e820_entry *entry, *new_entry;

	for (i = (int32_t)hv_e820_entries_nr - 1; i >= 0; i--) {
		entry = &hv_e820[i];
		uint64_t start, end, length;

		start = round_page_up(entry->baseaddr);
		end = round_page_down(entry->baseaddr + entry->length);
		length = (end > start) ? (end - start) : 0UL;

		if ((entry->type == E820_TYPE_RAM) && (length >= size) && ((start + size) <= max_addr)) {


			/* found exact size of e820 entry */
			if (length == size) {
				entry->type = E820_TYPE_RESERVED;
				ret = start;
			} else {

				/*
				 * found entry with available memory larger than requested (length > size)
				 * Reserve memory if
				 * 1) hv_e820_entries_nr < E820_MAX_ENTRIES
				 * 2) if end of this "entry" is <= max_addr
				 *    use memory from end of this e820 "entry".
				 */

				if ((hv_e820_entries_nr < E820_MAX_ENTRIES) && (end <= max_addr)) {

					new_entry = &hv_e820[hv_e820_entries_nr];
					new_entry->type = E820_TYPE_RESERVED;
					new_entry->baseaddr = end - size;
					new_entry->length = (entry->baseaddr + entry->length) - new_entry->baseaddr;
					/* Shrink the existing entry and total available memory */
					entry->length -= new_entry->length;
					hv_e820_entries_nr++;

				        ret = new_entry->baseaddr;
				}
			}

			if (ret != INVALID_HPA) {
				break;
			}
		}
	}

	if ((ret == INVALID_HPA) || (ret == 0UL)) {
		/* current memory allocation algorithm is to find the available address from the highest
		 * possible address below max_addr. if ret == 0, means all memory is used up and we have to
		 * put the resource at address 0, this is dangerous.
		 * Also ret == 0 would make code logic very complicated, since memcpy_s() doesn't support
		 * address 0 copy.
		 */
		panic("Requested memory from E820 cannot be reserved!!");
	}

	return ret;
}
/* HV read multiboot header to get e820 entries info and calc total RAM info */
void init_e820(void)
{
	uint32_t i;
	uint64_t top_addr_space = CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE;

	struct acrn_boot_info *abi = get_acrn_boot_info();
	struct multiboot_mmap *mmap = abi->mi_mmap_entry;

	hv_e820_entries_nr = abi->mi_mmap_entries;

	dev_dbg(DBG_LEVEL_E820, "mmap addr 0x%x entries %d\n",
		abi->mi_mmap_entry, hv_e820_entries_nr);


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

		dev_dbg(DBG_LEVEL_E820, "mmap table: %d type: 0x%x", i, mmap[i].type);
		dev_dbg(DBG_LEVEL_E820, "Base: 0x%016lx length: 0x%016lx\n",
			mmap[i].baseaddr, mmap[i].length);
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
