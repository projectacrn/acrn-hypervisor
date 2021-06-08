/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/pgtable.h>
#include <boot.h>
#include "multiboot_priv.h"

/**
 * @pre abi != NULL
 */
int32_t multiboot_to_acrn_bi(struct acrn_boot_info *abi, void *mb_info) {
	struct multiboot_info *mbi = (struct multiboot_info *)(hpa2hva_early((uint64_t)mb_info));
	struct multiboot_mmap *mmap = (struct multiboot_mmap *)hpa2hva_early((uint64_t)mbi->mi_mmap_addr);
	struct multiboot_module *mods = (struct multiboot_module *)hpa2hva_early((uint64_t)mbi->mi_mods_addr);

	(void)strncpy_s((void *)(abi->cmdline), MAX_BOOTARGS_SIZE, (char *)hpa2hva_early((uint64_t)mbi->mi_cmdline),
				strnlen_s((char *)hpa2hva_early((uint64_t)mbi->mi_cmdline), (MAX_BOOTARGS_SIZE - 1U)));

	(void)strncpy_s((void *)(abi->loader_name), MAX_LOADER_NAME_SIZE,
			(char *)hpa2hva_early((uint64_t)mbi->mi_loader_name),
			strnlen_s((char *)hpa2hva_early((uint64_t)mbi->mi_loader_name), (MAX_LOADER_NAME_SIZE - 1U)));

	abi->mi_mmap_entries = mbi->mi_mmap_length / sizeof(struct multiboot_mmap);
	abi->mi_mods_count = mbi->mi_mods_count;

	if (((mbi->mi_flags & MULTIBOOT_INFO_HAS_MMAP) != 0U) && (abi->mi_mmap_entries != 0U) && (mmap != NULL)) {

		if (abi->mi_mmap_entries > MAX_MMAP_ENTRIES) {
			abi->mi_mmap_entries = MAX_MMAP_ENTRIES;
		}

		(void)memcpy_s((void *)(&abi->mi_mmap_entry[0]),
			(abi->mi_mmap_entries * sizeof(struct multiboot_mmap)),
			mmap, (abi->mi_mmap_entries * sizeof(struct multiboot_mmap)));

	} else {
		abi->mi_mmap_entries = 0U;
	}

	if (((mbi->mi_flags & MULTIBOOT_INFO_HAS_MODS) != 0U) && (abi->mi_mods_count != 0U) && (mods != NULL)) {
		if (abi->mi_mods_count > MAX_MODULE_NUM) {
			abi->mi_mods_count = MAX_MODULE_NUM;
		}

		(void)memcpy_s((void *)(&abi->mi_mods[0]),
			(abi->mi_mods_count * sizeof(struct multiboot_module)),
			mods, (abi->mi_mods_count * sizeof(struct multiboot_module)));
	} else {
		abi->mi_mods_count = 0U;
	}

	return 0;
}

int32_t init_multiboot_info(uint32_t *registers)
{
	int32_t ret = -ENODEV;
	uint32_t magic = registers[0];
	uint32_t info = registers[1];
	struct acrn_boot_info *abi = get_acrn_boot_info();

	if (boot_from_multiboot(magic, info)) {
		if (multiboot_to_acrn_bi(abi, hpa2hva_early((uint64_t)info)) == 0) {
			strncpy_s(abi->protocol_name, MAX_PROTOCOL_NAME_SIZE,
					"Multiboot", (MAX_PROTOCOL_NAME_SIZE - 1U));
			ret = 0;
		}
#ifdef CONFIG_MULTIBOOT2
	} else if (boot_from_multiboot2(magic)) {
		if (multiboot2_to_acrn_bi(abi, hpa2hva_early((uint64_t)info)) == 0) {
			strncpy_s(abi->protocol_name, MAX_PROTOCOL_NAME_SIZE,
					"Multiboot2", (MAX_PROTOCOL_NAME_SIZE - 1U));
			ret = 0;
		}
#endif
	}
	return ret;
}
