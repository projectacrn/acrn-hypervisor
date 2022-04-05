/*
 * Copyright (C) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <acrn_hv_defs.h>
#include <asm/page.h>
#include <asm/e820.h>
#include <asm/mmu.h>
#include <boot.h>
#include <efi_mmap.h>
#include <logmsg.h>
#include <asm/guest/ept.h>

/*
 * e820.c contains the related e820 operations; like HV to get memory info for its MMU setup;
 * and hide HV memory from Service VM...
 */

static uint32_t hv_e820_entries_nr;
static uint64_t hv_e820_ram_size;
/* Describe the memory layout the hypervisor uses */
static struct e820_entry hv_e820[E820_MAX_ENTRIES];

#define DBG_LEVEL_E820	6U

/*
 * @brief reserve some RAM, hide it from Service VM, return its start address
 *
 * e820_alloc_memory requires 4k alignment, so size_arg will be converted
 * in the function.
 *
 * @param size_arg Amount of memory to be found and marked reserved
 * @param max_addr Maximum address below which memory is to be identified
 *
 * @pre hv_e820_entries_nr > 0U
 * @return base address of the memory region
 */
uint64_t e820_alloc_memory(uint64_t size_arg, uint64_t max_addr)
{
	int32_t i;
	uint64_t size = round_page_up(size_arg);
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

static void insert_e820_entry(uint32_t index, uint64_t addr, uint64_t length, uint64_t type)
{
	uint32_t i;

	hv_e820_entries_nr++;
	ASSERT(hv_e820_entries_nr <= E820_MAX_ENTRIES, "e820 entry overflow");

	for (i = hv_e820_entries_nr - 1; i > index; i--) {
		hv_e820[i] = hv_e820[i-1];
	}

	hv_e820[index].baseaddr = addr;
	hv_e820[index].length = length;
	hv_e820[index].type = type;
}

static uint64_t e820_alloc_region(uint64_t addr, uint64_t size)
{
	uint32_t i;
	uint64_t entry_start;
	uint64_t entry_end;
	uint64_t start_pa = round_page_down(addr);
	uint64_t end_pa = round_page_up(addr + size);
	struct e820_entry *entry;

	for (i = 0U; i < hv_e820_entries_nr; i++) {
		entry = &hv_e820[i];
		entry_start = entry->baseaddr;
		entry_end = entry->baseaddr + entry->length;

		/* No need handle in these cases*/
		if ((entry->type != E820_TYPE_RAM) || (entry_end <= start_pa) || (entry_start >= end_pa)) {
			continue;
		}

		if ((entry_start <= start_pa) && (entry_end >= end_pa)) {
			entry->length = start_pa - entry_start;
			/*
			 * .......|start_pa... ....................End_pa|.....
			 * |entry_start..............................entry_end|
			 */
			if (end_pa < entry_end) {
				insert_e820_entry(i + 1, end_pa, entry_end - end_pa, E820_TYPE_RAM);
				break;
			}
		} else {
			pr_err("This region not in one entry!");
		}
	}

	return addr;
}

static void init_e820_from_efi_mmap(void)
{
	uint32_t i, e820_idx = 0U;
	const struct efi_memory_desc *efi_mmap_entry = get_efi_mmap_entry();

	for (i = 0U; i < get_efi_mmap_entries_count(); i++) {
		if (e820_idx >= E820_MAX_ENTRIES) {
			pr_err("Too many efi memmap entries !");
			break;
		}

		hv_e820[e820_idx].baseaddr = efi_mmap_entry[i].phys_addr;
		hv_e820[e820_idx].length = efi_mmap_entry[i].num_pages * PAGE_SIZE;

		/* The EFI BOOT Service releated regions need to be set to reserved and avoid being touched by
		 * hypervisor, because at least below software modules rely on them:
		 * 1. EFI ESRT(The EFI System Resource Table) which used for UEFI firmware upgrade;
		 * 2. Image resource in ACPI BGRT(Boottime Graphics Resource Table) which used for boot time logo;
		 */
		switch (efi_mmap_entry[i].type)	{
		case EFI_LOADER_CODE:
		case EFI_LOADER_DATA:
		case EFI_CONVENTIONAL_MEMORY:
			if ((efi_mmap_entry[i].attribute & EFI_MEMORY_WB) != 0UL) {
				hv_e820[e820_idx].type = E820_TYPE_RAM;
			} else {
				hv_e820[e820_idx].type = E820_TYPE_RESERVED;
			}
			break;
		case EFI_UNUSABLE_MEMORY:
			hv_e820[e820_idx].type = E820_TYPE_UNUSABLE;
			break;
		case EFI_ACPI_RECLAIM_MEMORY:
			hv_e820[e820_idx].type = E820_TYPE_ACPI_RECLAIM;
			break;
		case EFI_ACPI_MEMORY_NVS:
			hv_e820[e820_idx].type = E820_TYPE_ACPI_NVS;
			break;
		/* case EFI_RESERVED_MEMORYTYPE:
		 * case EFI_BOOT_SERVICES_CODE:
		 * case EFI_BOOT_SERVICES_DATA:
		 * case EFI_RUNTIME_SERVICES_CODE:
		 * case EFI_RUNTIME_SERVICES_DATA:
		 * case EFI_MEMORYMAPPED_IO:
		 * case EFI_MEMORYMAPPED_IOPORTSPACE:
		 * case EFI_PALCODE:
		 * case EFI_PERSISTENT_MEMORY:
		 */
		default:
			hv_e820[e820_idx].type = E820_TYPE_RESERVED;
			break;
		}

		/* Given the efi memmap has been sorted, the hv_e820[] is also sorted.
		 * Then the algorithm is very simple, just merge with previous mmap entry
		 * if type is same and base addr is continuous.
		 */
		if ((e820_idx > 0U) && (hv_e820[e820_idx].type == hv_e820[e820_idx - 1U].type)
				&& (hv_e820[e820_idx].baseaddr ==
					(hv_e820[e820_idx - 1U].baseaddr
					+ hv_e820[e820_idx - 1U].length))) {
			hv_e820[e820_idx - 1U].length += hv_e820[e820_idx].length;
		} else {
			dev_dbg(DBG_LEVEL_E820, "efi mmap hv_e820[%d]: type: 0x%x Base: 0x%016lx length: 0x%016lx",
			    e820_idx, hv_e820[e820_idx].type, hv_e820[e820_idx].baseaddr, hv_e820[e820_idx].length);
			e820_idx ++;
		}

	}

	hv_e820_entries_nr = e820_idx;

}

/* HV read multiboot header to get e820 entries info and calc total RAM info */
static void init_e820_from_mmap(struct acrn_boot_info *abi)
{
	uint32_t i;

	struct abi_mmap *mmap = abi->mmap_entry;

	hv_e820_entries_nr = abi->mmap_entries;

	dev_dbg(DBG_LEVEL_E820, "mmap addr 0x%x entries %d\n",
		abi->mmap_entry, hv_e820_entries_nr);

	for (i = 0U; i < hv_e820_entries_nr; i++) {

		hv_e820[i].baseaddr = mmap[i].baseaddr;
		hv_e820[i].length = mmap[i].length;
		hv_e820[i].type = mmap[i].type;

		dev_dbg(DBG_LEVEL_E820, "mmap hv_e820[%d]: type: 0x%x Base: 0x%016lx length: 0x%016lx", i,
			mmap[i].type, mmap[i].baseaddr, mmap[i].length);
	}
}

static void calculate_e820_ram_size(void)
{
        uint32_t i;

        for(i = 0; i < hv_e820_entries_nr; i++){
                dev_dbg(DBG_LEVEL_E820, "hv_e820[%d]:type: 0x%x Base: 0x%016lx length: 0x%016lx", i,
                                hv_e820[i].type, hv_e820[i].baseaddr, hv_e820[i].length);

                if (hv_e820[i].type == E820_TYPE_RAM) {
                        hv_e820_ram_size += hv_e820[i].baseaddr + hv_e820[i].length;
                }
        }

        dev_dbg(DBG_LEVEL_E820, "ram size: 0x%016lx ",hv_e820_ram_size);
}

static void alloc_mods_memory(void)
{
	uint32_t i;
	int64_t mod_start = 0UL;
	struct acrn_boot_info *abi = get_acrn_boot_info();

	for (i = 0; i < abi->mods_count; i++) {
		mod_start = hva2hpa(abi->mods[i].start);
		e820_alloc_region(mod_start, abi->mods[i].size);
	}
}


void init_e820(void)
{
	struct acrn_boot_info *abi = get_acrn_boot_info();

	if (boot_from_uefi(abi)) {
		init_efi_mmap_entries(&abi->uefi_info);
		init_e820_from_efi_mmap();
	} else {
		init_e820_from_mmap(abi);
	}

	calculate_e820_ram_size();
	/* reserve multiboot modules memory */
	alloc_mods_memory();
}

uint64_t get_e820_ram_size(void)
{
        return hv_e820_ram_size;
}

uint32_t get_e820_entries_count(void)
{
	return hv_e820_entries_nr;
}

const struct e820_entry *get_e820_entry(void)
{
	return hv_e820;
}
