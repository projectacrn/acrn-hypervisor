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

/* The modules in multiboot are: Pre-launched VM: kernel/ramdisk/acpi; SOS VM: kernel/ramdisk */
#define MAX_MODULE_NUM			(3U * PRE_VM_NUM + 2U * SOS_VM_NUM)

/* The vACPI module size is fixed to 1MB */
#define ACPI_MODULE_SIZE		MEM_1M

struct acrn_boot_info {

	const char		*mi_cmdline;
	const char		*mi_loader_name;

	uint32_t		mi_mods_count;
	const void		*mi_mods_va;
	struct multiboot_module	mi_mods[MAX_MODULE_NUM];

	uint32_t 		mi_drives_length;
	uint32_t		mi_drives_addr;

	uint32_t		mi_mmap_entries;
	const void		*mi_mmap_va;
	struct multiboot_mmap	mi_mmap_entry[MAX_MMAP_ENTRIES];

	const void		*mi_acpi_rsdp_va;
	struct efi_info		mi_efi_info;
};

void init_acrn_boot_info(uint32_t magic, uint32_t info);
int32_t sanitize_acrn_boot_info(uint32_t magic, uint32_t info);
struct acrn_boot_info *get_acrn_boot_info(void);

#endif	/* BOOT_H */
