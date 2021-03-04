/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <multiboot.h>
#include <x86/pgtable.h>
#include "multiboot_priv.h"

/**
 * @pre mbi != NULL && mb2_tag_mmap != NULL
 */
static void mb2_mmap_to_mbi(struct acrn_multiboot_info *mbi, const struct multiboot2_tag_mmap *mb2_tag_mmap)
{
	/* multiboot2 mmap tag header occupied 16 bytes */
	mbi->mi_mmap_entries = (mb2_tag_mmap->size - 16U) / sizeof(struct multiboot2_mmap_entry);
	mbi->mi_mmap_va = (struct multiboot2_mmap_entry *)mb2_tag_mmap->entries;
}

/**
 * @pre mbi != NULL && mb2_tag_mods != NULL
 */
static void mb2_mods_to_mbi(struct acrn_multiboot_info *mbi,
			uint32_t mbi_mod_idx, const struct multiboot2_tag_module *mb2_tag_mods)
{
	if (mbi_mod_idx < MAX_MODULE_NUM) {
		mbi->mi_mods[mbi_mod_idx].mm_mod_start = mb2_tag_mods->mod_start;
		mbi->mi_mods[mbi_mod_idx].mm_mod_end = mb2_tag_mods->mod_end;
		mbi->mi_mods[mbi_mod_idx].mm_string = (uint32_t)(uint64_t)mb2_tag_mods->cmdline;
	}
}

/**
 * @pre mbi != NULL && mb2_tag_efi64 != 0
 */
static void mb2_efi64_to_mbi(struct acrn_multiboot_info *mbi, const struct multiboot2_tag_efi64 *mb2_tag_efi64)
{
	const uint32_t efiloader_sig = 0x34364c45; /* "EL64" */
	mbi->mi_efi_info.efi_systab = (uint32_t)(uint64_t)mb2_tag_efi64->pointer;
	mbi->mi_efi_info.efi_loader_signature = efiloader_sig;
	mbi->mi_flags |= MULTIBOOT_INFO_HAS_EFI64;
}

/**
 * @pre mbi != NULL && mb2_tag_efimmap != 0
 */
static void mb2_efimmap_to_mbi(struct acrn_multiboot_info *mbi,
			const struct multiboot2_tag_efi_mmap *mb2_tag_efimmap)
{
	mbi->mi_efi_info.efi_memdesc_size = mb2_tag_efimmap->descr_size;
	mbi->mi_efi_info.efi_memdesc_version = mb2_tag_efimmap->descr_vers;
	mbi->mi_efi_info.efi_memmap = (uint32_t)(uint64_t)mb2_tag_efimmap->efi_mmap;
	mbi->mi_efi_info.efi_memmap_size = mb2_tag_efimmap->size - 16U;
	mbi->mi_efi_info.efi_memmap_hi = (uint32_t)(((uint64_t)mb2_tag_efimmap->efi_mmap) >> 32U);
	mbi->mi_flags |= MULTIBOOT_INFO_HAS_EFI_MMAP;
}

/**
 * @pre mbi != NULL
 */
int32_t multiboot2_to_acrn_mbi(struct acrn_multiboot_info *mbi, void *mb2_info)
{
	int32_t ret = 0;
	struct multiboot2_tag *mb2_tag, *mb2_tag_end;
	uint32_t mb2_info_size = *(uint32_t *)mb2_info;
	uint32_t mod_idx = 0U;

	/* The start part of multiboot2 info: total mbi size (4 bytes), reserved (4 bytes) */
	mb2_tag = (struct multiboot2_tag *)((uint8_t *)mb2_info + 8U);
	mb2_tag_end = (struct multiboot2_tag *)((uint8_t *)mb2_info + mb2_info_size);

	while ((mb2_tag->type != MULTIBOOT2_TAG_TYPE_END) && (mb2_tag < mb2_tag_end)) {
		switch (mb2_tag->type) {
		case MULTIBOOT2_TAG_TYPE_CMDLINE:
			mbi->mi_cmdline = ((struct multiboot2_tag_string *)mb2_tag)->string;
			mbi->mi_flags |= MULTIBOOT_INFO_HAS_CMDLINE;
			break;
		case MULTIBOOT2_TAG_TYPE_MMAP:
			mb2_mmap_to_mbi(mbi, (const struct multiboot2_tag_mmap *)mb2_tag);
			break;
		case MULTIBOOT2_TAG_TYPE_MODULE:
			mb2_mods_to_mbi(mbi, mod_idx, (const struct multiboot2_tag_module *)mb2_tag);
			mod_idx++;
			break;
		case MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME:
			mbi->mi_loader_name = ((struct multiboot2_tag_string *)mb2_tag)->string;
			break;
		case MULTIBOOT2_TAG_TYPE_ACPI_NEW:
			mbi->mi_acpi_rsdp_va = ((struct multiboot2_tag_new_acpi *)mb2_tag)->rsdp;
			break;
		case MULTIBOOT2_TAG_TYPE_EFI64:
			mb2_efi64_to_mbi(mbi, (const struct multiboot2_tag_efi64 *)mb2_tag);
			break;
		case MULTIBOOT2_TAG_TYPE_EFI_MMAP:
			mb2_efimmap_to_mbi(mbi, (const struct multiboot2_tag_efi_mmap *)mb2_tag);
			break;
		default:
			if (mb2_tag->type > MULTIBOOT2_TAG_TYPE_LOAD_BASE_ADDR) {
				ret = -EINVAL;
			}
			break;
		}
		if (mb2_tag->size == 0U) {
			ret = -EINVAL;
		}

		if (ret != 0) {
			break;
		}
		/*
		 * tag->size does not include padding whearas each tag
		 * start at 8-bytes aligned address.
		 */
		mb2_tag = (struct multiboot2_tag *)((uint8_t *)mb2_tag
				+ ((mb2_tag->size + (MULTIBOOT2_INFO_ALIGN - 1U)) & ~(MULTIBOOT2_INFO_ALIGN - 1U)));
	}

	mbi->mi_mods_count = mod_idx;

	return ret;
}
