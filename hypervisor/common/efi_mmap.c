/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <boot.h>
#include <efi.h>
#include <efi_mmap.h>
#include <logmsg.h>

static uint32_t hv_memdesc_nr;
static struct efi_memory_desc hv_memdesc[CONFIG_MAX_EFI_MMAP_ENTRIES];

static void sort_efi_mmap_entries(void)
{
	uint32_t i, j;
	struct efi_memory_desc tmp_memdesc;

	/* Bubble sort */
	for (i = 0U; i < (hv_memdesc_nr - 1U); i++) {
		for (j = 0U; j < (hv_memdesc_nr - i - 1U); j++) {
			if (hv_memdesc[j].phys_addr > hv_memdesc[j + 1U].phys_addr) {
				tmp_memdesc = hv_memdesc[j];
				hv_memdesc[j] = hv_memdesc[j + 1U];
				hv_memdesc[j + 1U] = tmp_memdesc;
			}
		}
	}
}

void init_efi_mmap_entries(struct efi_info *uefi_info)
{
	void *efi_memmap = (void *)((uint64_t)uefi_info->memmap | ((uint64_t)uefi_info->memmap_hi << 32U));
	struct efi_memory_desc *efi_memdesc = (struct efi_memory_desc *)efi_memmap;
	uint32_t entry = 0U;

	while ((void *)efi_memdesc < (efi_memmap + uefi_info->memmap_size)) {
		if (entry >= CONFIG_MAX_EFI_MMAP_ENTRIES) {
			pr_err("Too many efi memmap entries, entries up %d are ignored.", CONFIG_MAX_EFI_MMAP_ENTRIES);
			break;
		}

		hv_memdesc[entry] = *efi_memdesc;

		/* Per UEFI spec, EFI_MEMORY_DESCRIPTOR array element returned in MemoryMap.
		 * The size is returned to allow for future expansion of the EFI_MEMORY_DESCRIPTOR
		 * in response to hardware innovation. The structure of the EFI_MEMORY_DESCRIPTOR
		 * may be extended in the future but it will remain backwards compatible with the
		 * current definition. Thus OS software must use the DescriptorSize to find the
		 * start of each EFI_MEMORY_DESCRIPTOR in the MemoryMap array.
		 */
		efi_memdesc = (struct efi_memory_desc *)((void *)efi_memdesc + uefi_info->memdesc_size);
		entry ++;
	}

	hv_memdesc_nr = entry;

	sort_efi_mmap_entries();
}

uint32_t get_efi_mmap_entries_count(void)
{
	return hv_memdesc_nr;
}

const struct efi_memory_desc *get_efi_mmap_entry(void)
{
	return hv_memdesc;
}
