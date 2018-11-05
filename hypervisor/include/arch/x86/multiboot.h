/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#define	MULTIBOOT_INFO_MAGIC		0x2BADB002U
#define	MULTIBOOT_INFO_HAS_CMDLINE	0x00000004U
#define	MULTIBOOT_INFO_HAS_MODS		0x00000008U
#define	MULTIBOOT_INFO_HAS_MMAP		0x00000040U
#define	MULTIBOOT_INFO_HAS_DRIVES	0x00000080U

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

int parse_hv_cmdline(void);
int init_vm_boot_info(struct acrn_vm *vm);
#endif
