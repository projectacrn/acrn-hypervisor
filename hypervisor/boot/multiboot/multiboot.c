/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <x86/pgtable.h>
#include <multiboot.h>
#include <rtl.h>
#include <logmsg.h>
#include "multiboot_priv.h"

static struct acrn_multiboot_info acrn_mbi = { 0U };

static int32_t mbi_status;

void init_acrn_multiboot_info(uint32_t magic, uint32_t info)
{
	if (boot_from_multiboot1(magic, info)) {
		struct multiboot_info *mbi = (struct multiboot_info *)(hpa2hva_early((uint64_t)info));

		acrn_mbi.mi_flags = mbi->mi_flags;
		acrn_mbi.mi_drives_addr = mbi->mi_drives_addr;
		acrn_mbi.mi_drives_length = mbi->mi_drives_length;
		acrn_mbi.mi_cmdline = (char *)hpa2hva_early((uint64_t)mbi->mi_cmdline);
		acrn_mbi.mi_loader_name = (char *)hpa2hva_early((uint64_t)mbi->mi_loader_name);
		acrn_mbi.mi_mmap_entries = mbi->mi_mmap_length / sizeof(struct multiboot_mmap);
		acrn_mbi.mi_mmap_va = (struct multiboot_mmap *)hpa2hva_early((uint64_t)mbi->mi_mmap_addr);
		acrn_mbi.mi_mods_count = mbi->mi_mods_count;
		acrn_mbi.mi_mods_va = (struct multiboot_module *)hpa2hva_early((uint64_t)mbi->mi_mods_addr);
		mbi_status = 0;
#ifdef CONFIG_MULTIBOOT2
	} else if (boot_from_multiboot2(magic)) {
		mbi_status = multiboot2_to_acrn_mbi(&acrn_mbi, hpa2hva_early((uint64_t)info));
#endif
	} else {
		mbi_status = -ENODEV;
	}
}

int32_t sanitize_acrn_multiboot_info(uint32_t magic, uint32_t info)
{
	if ((acrn_mbi.mi_mmap_entries != 0U) && (acrn_mbi.mi_mmap_va != NULL)) {
		if (acrn_mbi.mi_mmap_entries > MAX_MMAP_ENTRIES) {
			pr_err("Too many E820 entries %d\n", acrn_mbi.mi_mmap_entries);
			acrn_mbi.mi_mmap_entries = MAX_MMAP_ENTRIES;
		}
		if (boot_from_multiboot1(magic, info)) {
			uint32_t mmap_entry_size = sizeof(struct multiboot_mmap);

			(void)memcpy_s((void *)(&acrn_mbi.mi_mmap_entry[0]),
				(acrn_mbi.mi_mmap_entries * mmap_entry_size),
				(const void *)acrn_mbi.mi_mmap_va,
				(acrn_mbi.mi_mmap_entries * mmap_entry_size));
		}
#ifdef CONFIG_MULTIBOOT2
		if (boot_from_multiboot2(magic)) {
			uint32_t i;
			struct multiboot2_mmap_entry *mb2_mmap = (struct multiboot2_mmap_entry *)acrn_mbi.mi_mmap_va;

			for (i = 0U; i < acrn_mbi.mi_mmap_entries; i++) {
				acrn_mbi.mi_mmap_entry[i].baseaddr = (mb2_mmap + i)->addr;
				acrn_mbi.mi_mmap_entry[i].length = (mb2_mmap + i)->len;
				acrn_mbi.mi_mmap_entry[i].type = (mb2_mmap + i)->type;
			}
		}
#endif
		acrn_mbi.mi_flags |= MULTIBOOT_INFO_HAS_MMAP;
	} else {
		acrn_mbi.mi_flags &= ~MULTIBOOT_INFO_HAS_MMAP;
	}

	if (acrn_mbi.mi_mods_count > MAX_MODULE_NUM) {
		pr_err("Too many multiboot modules %d\n", acrn_mbi.mi_mods_count);
		acrn_mbi.mi_mods_count = MAX_MODULE_NUM;
	}
	if (acrn_mbi.mi_mods_count != 0U) {
		if (boot_from_multiboot1(magic, info) && (acrn_mbi.mi_mods_va != NULL)) {
			(void)memcpy_s((void *)(&acrn_mbi.mi_mods[0]),
				(acrn_mbi.mi_mods_count * sizeof(struct multiboot_module)),
				(const void *)acrn_mbi.mi_mods_va,
				(acrn_mbi.mi_mods_count * sizeof(struct multiboot_module)));
		}
		acrn_mbi.mi_flags |= MULTIBOOT_INFO_HAS_MODS;
	} else {
		acrn_mbi.mi_flags &= ~MULTIBOOT_INFO_HAS_MODS;
	}

	if ((acrn_mbi.mi_flags & MULTIBOOT_INFO_HAS_MMAP) == 0U) {
		pr_err("wrong multiboot flags: 0x%08x", acrn_mbi.mi_flags);
		mbi_status = -EINVAL;
	}

#ifdef CONFIG_MULTIBOOT2
	if (boot_from_multiboot2(magic)) {
		if (acrn_mbi.mi_efi_info.efi_memmap_hi != 0U) {
			pr_err("the EFI mmap address should be less than 4G!");
			acrn_mbi.mi_flags &= ~MULTIBOOT_INFO_HAS_EFI_MMAP;
			mbi_status = -EINVAL;
		}

		if ((acrn_mbi.mi_flags & (MULTIBOOT_INFO_HAS_EFI64 | MULTIBOOT_INFO_HAS_EFI_MMAP)) == 0U) {
			pr_err("no multiboot2 uefi info found!");
		}
	}
#endif

	if (acrn_mbi.mi_loader_name[0] == '\0') {
		pr_err("no bootloader name found!");
		mbi_status = -EINVAL;
	} else {
		printf("Multiboot%s Bootloader: %s\n", boot_from_multiboot1(magic, info) ? "" : "2", acrn_mbi.mi_loader_name);
	}

	return mbi_status;
}

/*
 * @post retval != NULL
 * @post retval->mi_flags & MULTIBOOT_INFO_HAS_MMAP != 0U
 * @post (retval->mi_mmap_entries > 0U) && (retval->mi_mmap_entries <= MAX_MMAP_ENTRIES)
 */
struct acrn_multiboot_info *get_acrn_multiboot_info(void)
{
	return &acrn_mbi;
}
