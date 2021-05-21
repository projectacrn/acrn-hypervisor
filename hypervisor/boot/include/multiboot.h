/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <multiboot_std.h>

/* TODO: MAX_MMAP_ENTRIES shall be config by config tool, and same as E820_MAX_ENTRIES */
#define MAX_MMAP_ENTRIES		32U

#define MAX_BOOTARGS_SIZE		2048U

/* The modules in multiboot are: Pre-launched VM: kernel/ramdisk/acpi; SOS VM: kernel/ramdisk */
#define MAX_MODULE_NUM			(3U * PRE_VM_NUM + 2U * SOS_VM_NUM)

/* The vACPI module size is fixed to 1MB */
#define ACPI_MODULE_SIZE		MEM_1M

#ifndef ASSEMBLER

#include <efi.h>
#include <vm_configurations.h>

struct acrn_boot_info {
	uint32_t		mi_flags;	/* the flags is back-compatible with multiboot1 */

	const char		*mi_cmdline;
	const char		*mi_loader_name;

	uint32_t		mi_mods_count;
	struct multiboot_module	mi_mods[MAX_MODULE_NUM];

	uint32_t 		mi_drives_length;
	uint32_t		mi_drives_addr;

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

#endif	/* ASSEMBLER */

#endif	/* MULTIBOOT_H */
