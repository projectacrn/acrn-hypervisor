/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#ifdef CONFIG_MULTIBOOT2
#include <multiboot2.h>
#endif

#define	MULTIBOOT_HEADER_MAGIC		0x1BADB002
#define	MULTIBOOT_INFO_MAGIC		0x2BADB002U

/* MULTIBOOT HEADER FLAGS */
#define	MULTIBOOT_HEADER_NEED_MEMINFO	0x00000002

/* MULTIBOOT INFO FLAGS */
#define	MULTIBOOT_INFO_HAS_CMDLINE	0x00000004U
#define	MULTIBOOT_INFO_HAS_MODS		0x00000008U
#define	MULTIBOOT_INFO_HAS_MMAP		0x00000040U
#define	MULTIBOOT_INFO_HAS_DRIVES	0x00000080U
#define	MULTIBOOT_INFO_HAS_LOADER_NAME	0x00000200U

/* extended flags for acrn multiboot info from multiboot2  */
#define	MULTIBOOT_INFO_HAS_EFI_MMAP	0x00010000U
#define	MULTIBOOT_INFO_HAS_EFI64	0x00020000U

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

struct multiboot_mmap {
	uint32_t size;
	uint64_t baseaddr;
	uint64_t length;
	uint32_t type;
} __packed;

struct multiboot_module {
	uint32_t	mm_mod_start;
	uint32_t	mm_mod_end;
	uint32_t	mm_string;
	uint32_t	mm_reserved;
};

struct acrn_multiboot_info {
	uint32_t		mi_flags;	/* the flags is back-compatible with multiboot1 */

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

void init_acrn_multiboot_info(uint32_t magic, uint32_t info, char *sig);
int32_t sanitize_acrn_multiboot_info(uint32_t magic, uint32_t info);
struct acrn_multiboot_info *get_acrn_multiboot_info(void);

#endif	/* ASSEMBLER */

#endif	/* MULTIBOOT_H */
