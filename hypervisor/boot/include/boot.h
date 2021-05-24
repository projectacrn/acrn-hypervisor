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

struct acrn_boot_info {

	const char		cmdline[MAX_BOOTARGS_SIZE];
	const char		loader_name[MAX_LOADER_NAME_SIZE];

	uint32_t		mods_count;
	struct abi_module	mods[MAX_MODULE_NUM];

	uint32_t		mi_mmap_entries;
	struct multiboot_mmap	mi_mmap_entry[MAX_MMAP_ENTRIES];

	const void		*mi_acpi_rsdp_va;
	struct efi_info		mi_efi_info;
};

int32_t init_multiboot_info(uint32_t *registers);
int32_t init_multiboot2_info(uint32_t *registers);

void init_acrn_boot_info(uint32_t *registers);
int32_t sanitize_acrn_boot_info(struct acrn_boot_info *abi);
struct acrn_boot_info *get_acrn_boot_info(void);

#endif	/* BOOT_H */
