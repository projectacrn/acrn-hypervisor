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

#define MAX_BOOTARGS_SIZE		2048U
/* The modules in multiboot are: Pre-launched VM: kernel/ramdisk/acpi; SOS VM: kernel/ramdisk */
#define MAX_MODULE_NUM			(3U * PRE_VM_NUM + 2U * SOS_VM_NUM)

/* The vACPI module size is fixed to 1MB */
#define ACPI_MODULE_SIZE		MEM_1M

#ifndef ASSEMBLER

#include <e820.h>
#include <zeropage.h>
#include <vm_configurations.h>

struct multiboot_info {
	uint32_t               mi_flags;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MEMORY. */
	uint32_t               mi_mem_lower;
	uint32_t               mi_mem_upper;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_BOOT_DEVICE. */
	uint8_t                 mi_boot_device_part3;
	uint8_t                 mi_boot_device_part2;
	uint8_t                 mi_boot_device_part1;
	uint8_t                 mi_boot_device_drive;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_CMDLINE. */
	uint32_t                mi_cmdline;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MODS. */
	uint32_t               mi_mods_count;
	uint32_t               mi_mods_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_{AOUT,ELF}_SYMS. */
	uint32_t               mi_elfshdr_num;
	uint32_t               mi_elfshdr_size;
	uint32_t               mi_elfshdr_addr;
	uint32_t               mi_elfshdr_shndx;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MMAP. */
	uint32_t               mi_mmap_length;
	uint32_t               mi_mmap_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_DRIVES. */
	uint32_t               mi_drives_length;
	uint32_t               mi_drives_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_CONFIG_TABLE. */
	uint32_t               unused_mi_config_table;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_LOADER_NAME. */
	uint32_t               mi_loader_name;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_APM. */
	uint32_t               unused_mi_apm_table;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_VBE. */
	uint32_t               unused_mi_vbe_control_info;
	uint32_t               unused_mi_vbe_mode_info;
	uint32_t               unused_mi_vbe_interface_seg;
	uint32_t               unused_mi_vbe_interface_off;
	uint32_t               unused_mi_vbe_interface_len;
};

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
	struct multiboot_mmap	mi_mmap_entry[E820_MAX_ENTRIES];

	const void		*mi_acpi_rsdp_va;
	struct efi_info		mi_efi_info;
};

/*
 * The extern declaration for acrn_mbi is for cmdline.c use only, other functions should use
 * get_multiboot_info() API to access struct acrn_mbi because it has explict @post condition
 */
extern struct acrn_multiboot_info acrn_mbi;

void init_acrn_multiboot_info(void);
struct acrn_multiboot_info *get_multiboot_info(void);
int32_t sanitize_multiboot_info(void);

void parse_hv_cmdline(void);
const void* get_rsdp_ptr(void);

#endif	/* ASSEMBLER */

#endif	/* MULTIBOOT_H */
