/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOOT_H
#define BOOT_H

#include <multiboot_std.h>
#include <efi.h>
#include <vm_configurations.h>

/* TODO: MAX_MMAP_ENTRIES shall be config by config tool, and same as E820_MAX_ENTRIES */
#define MAX_MMAP_ENTRIES		32U

#define MAX_BOOTARGS_SIZE		2048U
#define MAX_LOADER_NAME_SIZE		32U
#define MAX_PROTOCOL_NAME_SIZE		16U
#define MAX_MOD_STRING_SIZE		2048U

/* The modules in multiboot are: Pre-launched VM: kernel/ramdisk/acpi; SOS VM: kernel/ramdisk */
#define MAX_MODULE_NUM			(3U * PRE_VM_NUM + 2U * SOS_VM_NUM)

/* The vACPI module size is fixed to 1MB */
#define ACPI_MODULE_SIZE		MEM_1M

struct abi_module {
	void			*start;		/* HVA */
	uint32_t		size;
	const char		string[MAX_MOD_STRING_SIZE];
};

/* ABI memory map types, compatible to Multiboot/Multiboot2/E820; */
#define MMAP_TYPE_RAM		1U
#define MMAP_TYPE_RESERVED	2U
#define MMAP_TYPE_ACPI_RECLAIM	3U
#define MMAP_TYPE_ACPI_NVS	4U
#define MMAP_TYPE_UNUSABLE	5U

struct abi_mmap {
	uint64_t		baseaddr;
	uint64_t		length;
	uint32_t		type;
};

struct acrn_boot_info {

	char			protocol_name[MAX_PROTOCOL_NAME_SIZE];
	const char		cmdline[MAX_BOOTARGS_SIZE];
	const char		loader_name[MAX_LOADER_NAME_SIZE];

	uint32_t		mods_count;
	struct abi_module	mods[MAX_MODULE_NUM];

	uint32_t		mmap_entries;
	struct abi_mmap		mmap_entry[MAX_MMAP_ENTRIES];

	const void		*acpi_rsdp_va;
	struct efi_info		uefi_info;
};

static inline bool boot_from_uefi(struct acrn_boot_info *abi)
{
	return !((abi->uefi_info.systab == 0U) && (abi->uefi_info.systab_hi == 0U));
}

int32_t init_multiboot_info(uint32_t *registers);

void init_acrn_boot_info(uint32_t *registers);
int32_t sanitize_acrn_boot_info(struct acrn_boot_info *abi);
struct acrn_boot_info *get_acrn_boot_info(void);

#endif	/* BOOT_H */
