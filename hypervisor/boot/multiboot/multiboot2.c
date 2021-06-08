/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <boot.h>
#include <asm/pgtable.h>
#include "multiboot_priv.h"

/**
 * @pre abi != NULL && mb2_tag_mmap != NULL
 */
static void mb2_mmap_to_abi(struct acrn_boot_info *abi, const struct multiboot2_tag_mmap *mb2_tag_mmap)
{
	uint32_t i;
	struct multiboot2_mmap_entry *mb2_mmap = (struct multiboot2_mmap_entry *)mb2_tag_mmap->entries;

	/* multiboot2 mmap tag header occupied 16 bytes */
	abi->mmap_entries = (mb2_tag_mmap->size - 16U) / sizeof(struct multiboot2_mmap_entry);
	if (abi->mmap_entries > MAX_MMAP_ENTRIES) {
		abi->mmap_entries = MAX_MMAP_ENTRIES;
	}

	for (i = 0U; i < abi->mmap_entries; i++) {
		abi->mmap_entry[i].baseaddr = (mb2_mmap + i)->addr;
		abi->mmap_entry[i].length = (mb2_mmap + i)->len;
		abi->mmap_entry[i].type = (mb2_mmap + i)->type;
	}
}

/**
 * @pre abi != NULL && mb2_tag_mods != NULL
 */
static void mb2_mods_to_abi(struct acrn_boot_info *abi,
			uint32_t mbi_mod_idx, const struct multiboot2_tag_module *mb2_tag_mods)
{
	abi->mods[mbi_mod_idx].start = hpa2hva_early((uint64_t)mb2_tag_mods->mod_start);
	if (mb2_tag_mods->mod_end > mb2_tag_mods->mod_start) {
		abi->mods[mbi_mod_idx].size = mb2_tag_mods->mod_end - mb2_tag_mods->mod_start;
	}

	(void)strncpy_s((void *)(abi->mods[mbi_mod_idx].string), MAX_MOD_STRING_SIZE,
		(char *)hpa2hva_early((uint64_t)mb2_tag_mods->cmdline),
		strnlen_s((char *)hpa2hva_early((uint64_t)mb2_tag_mods->cmdline), MAX_MOD_STRING_SIZE));
}

/**
 * @pre abi != NULL && mb2_tag_efi64 != 0
 */
static void mb2_efi64_to_abi(struct acrn_boot_info *abi, const struct multiboot2_tag_efi64 *mb2_tag_efi64)
{
	const uint32_t efiloader_sig = 0x34364c45; /* "EL64" */
	abi->mi_efi_info.efi_systab = (uint32_t)(uint64_t)mb2_tag_efi64->pointer;
	abi->mi_efi_info.efi_systab_hi = (uint32_t)((uint64_t)mb2_tag_efi64->pointer >> 32U);
	abi->mi_efi_info.efi_loader_signature = efiloader_sig;
}

/**
 * @pre abi != NULL && mb2_tag_efimmap != 0
 */
static void mb2_efimmap_to_abi(struct acrn_boot_info *abi,
			const struct multiboot2_tag_efi_mmap *mb2_tag_efimmap)
{
	abi->mi_efi_info.efi_memdesc_size = mb2_tag_efimmap->descr_size;
	abi->mi_efi_info.efi_memdesc_version = mb2_tag_efimmap->descr_vers;
	abi->mi_efi_info.efi_memmap = (uint32_t)(uint64_t)mb2_tag_efimmap->efi_mmap;
	abi->mi_efi_info.efi_memmap_size = mb2_tag_efimmap->size - 16U;
	abi->mi_efi_info.efi_memmap_hi = (uint32_t)(((uint64_t)mb2_tag_efimmap->efi_mmap) >> 32U);
}

/**
 * @pre abi != NULL
 */
int32_t multiboot2_to_acrn_bi(struct acrn_boot_info *abi, void *mb2_info)
{
	int32_t ret = 0;
	struct multiboot2_tag *mb2_tag, *mb2_tag_end;
	uint32_t mb2_info_size = *(uint32_t *)mb2_info;
	uint32_t mod_idx = 0U;
	void *str;

	/* The start part of multiboot2 info: total mbi size (4 bytes), reserved (4 bytes) */
	mb2_tag = (struct multiboot2_tag *)((uint8_t *)mb2_info + 8U);
	mb2_tag_end = (struct multiboot2_tag *)((uint8_t *)mb2_info + mb2_info_size);

	while ((mb2_tag->type != MULTIBOOT2_TAG_TYPE_END) && (mb2_tag < mb2_tag_end)) {
		switch (mb2_tag->type) {
		case MULTIBOOT2_TAG_TYPE_CMDLINE:
			str = ((struct multiboot2_tag_string *)mb2_tag)->string;
			(void)strncpy_s((void *)(abi->cmdline), MAX_BOOTARGS_SIZE, str,
						strnlen_s(str, (MAX_BOOTARGS_SIZE - 1U)));
			break;
		case MULTIBOOT2_TAG_TYPE_MMAP:
			mb2_mmap_to_abi(abi, (const struct multiboot2_tag_mmap *)mb2_tag);
			break;
		case MULTIBOOT2_TAG_TYPE_MODULE:
			if (mod_idx < MAX_MODULE_NUM) {
				mb2_mods_to_abi(abi, mod_idx, (const struct multiboot2_tag_module *)mb2_tag);
				mod_idx++;
			}
			break;
		case MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME:
			str = ((struct multiboot2_tag_string *)mb2_tag)->string;
			(void)strncpy_s((void *)(abi->loader_name), MAX_LOADER_NAME_SIZE, str,
						strnlen_s(str, (MAX_LOADER_NAME_SIZE - 1U)));
			break;
		case MULTIBOOT2_TAG_TYPE_ACPI_NEW:
			abi->mi_acpi_rsdp_va = ((struct multiboot2_tag_new_acpi *)mb2_tag)->rsdp;
			break;
		case MULTIBOOT2_TAG_TYPE_EFI64:
			mb2_efi64_to_abi(abi, (const struct multiboot2_tag_efi64 *)mb2_tag);
			break;
		case MULTIBOOT2_TAG_TYPE_EFI_MMAP:
			mb2_efimmap_to_abi(abi, (const struct multiboot2_tag_efi_mmap *)mb2_tag);
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

	abi->mods_count = mod_idx;

	return ret;
}
