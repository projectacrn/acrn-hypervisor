/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <pgtable.h>
#include <boot.h>
#include <rtl.h>
#include <logmsg.h>

static struct acrn_multiboot_info acrn_mbi = { 0U };

int32_t sanitize_multiboot_info(void)
{
	int32_t ret = 0;

	if (boot_from_multiboot1()) {
		struct multiboot_info *mbi = (struct multiboot_info *)(hpa2hva_early((uint64_t)boot_regs[1]));

		pr_info("Multiboot1 detected.");
		acrn_mbi.mi_flags = mbi->mi_flags;
		acrn_mbi.mi_drives_addr = mbi->mi_drives_addr;
		acrn_mbi.mi_drives_length = mbi->mi_drives_length;
		acrn_mbi.mi_cmdline = (char *)hpa2hva_early((uint64_t)mbi->mi_cmdline);
		acrn_mbi.mi_loader_name = (char *)hpa2hva_early((uint64_t)mbi->mi_loader_name);

		acrn_mbi.mi_mmap_entries = mbi->mi_mmap_length / sizeof(struct multiboot_mmap);
		if ((acrn_mbi.mi_mmap_entries != 0U) && (mbi->mi_mmap_addr != 0U)) {
			if (acrn_mbi.mi_mmap_entries > E820_MAX_ENTRIES) {
				pr_err("Too many E820 entries %d\n", acrn_mbi.mi_mmap_entries);
				acrn_mbi.mi_mmap_entries = E820_MAX_ENTRIES;
			}
			(void)memcpy_s((void *)(&acrn_mbi.mi_mmap_entry[0]),
				(acrn_mbi.mi_mmap_entries * sizeof(struct multiboot_mmap)),
				(const void *)hpa2hva_early((uint64_t)mbi->mi_mmap_addr),
				(acrn_mbi.mi_mmap_entries * sizeof(struct multiboot_mmap)));
		} else {
			acrn_mbi.mi_flags &= ~MULTIBOOT_INFO_HAS_MMAP;
		}

		acrn_mbi.mi_mods_count = mbi->mi_mods_count;
		if ((mbi->mi_mods_count != 0U) && (mbi->mi_mods_addr != 0U)) {
			if (mbi->mi_mods_count > MAX_MODULE_COUNT) {
				pr_err("Too many multiboot modules %d\n", mbi->mi_mods_count);
				acrn_mbi.mi_mods_count = MAX_MODULE_COUNT;
			}
			(void)memcpy_s((void *)(&acrn_mbi.mi_mods[0]),
				(acrn_mbi.mi_mods_count * sizeof(struct multiboot_module)),
				(const void *)hpa2hva_early((uint64_t)mbi->mi_mods_addr),
				(acrn_mbi.mi_mods_count * sizeof(struct multiboot_module)));
		} else {
			acrn_mbi.mi_flags &= ~MULTIBOOT_INFO_HAS_MODS;
		}
#ifdef CONFIG_MULTIBOOT2
	} else if (boot_from_multiboot2()) {
		ret = multiboot2_to_acrn_mbi(&acrn_mbi, hpa2hva_early((uint64_t)boot_regs[1]));
#endif
	} else {
		pr_err("no multiboot info found!");
		ret = -ENODEV;
	}

	if ((acrn_mbi.mi_flags & MULTIBOOT_INFO_HAS_MMAP) == 0U) {
		pr_err("no multiboot memory map info found!");
		ret = -EINVAL;
	}

	if (acrn_mbi.mi_loader_name[0] == '\0') {
		pr_err("no bootloader name found!");
		ret = -EINVAL;
	}
	return ret;
}

/*
 * @post retval != NULL
 * @post retval->mi_flags & MULTIBOOT_INFO_HAS_MMAP != 0U
 * @post (retval->mi_mmap_entries > 0U) && (retval->mi_mmap_entries <= E820_MAX_ENTRIES)
 */
struct acrn_multiboot_info *get_multiboot_info(void)
{
	return &acrn_mbi;
}
