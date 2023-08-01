/*
 * Copyright (C) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* multiboot_std.h - Multiboot standard header file. */
/*
 * Reference:
 * https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 * https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html
 */

/*   Copyright (C) 1999,2003,2007,2008,2009,2010  Free Software Foundation, Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL ANY
 *  DEVELOPER OR DISTRIBUTOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 *  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MULTIBOOT_STD_H
#define MULTIBOOT_STD_H

#define	MULTIBOOT_HEADER_MAGIC				0x1BADB002
#define	MULTIBOOT_INFO_MAGIC				0x2BADB002U

/* MULTIBOOT HEADER FLAGS */
#define	MULTIBOOT_HEADER_NEED_MEMINFO			0x00000002

/* MULTIBOOT INFO FLAGS */
#define	MULTIBOOT_INFO_HAS_CMDLINE			0x00000004U
#define	MULTIBOOT_INFO_HAS_MODS				0x00000008U
#define	MULTIBOOT_INFO_HAS_MMAP				0x00000040U
#define	MULTIBOOT_INFO_HAS_DRIVES			0x00000080U
#define	MULTIBOOT_INFO_HAS_LOADER_NAME			0x00000200U

#ifndef ASSEMBLER

struct multiboot_header {
	/* Must be MULTIBOOT_MAGIC - see above. */
	uint32_t magic;

	/* Feature flags. */
	uint32_t flags;

	/* The above fields plus this one must equal 0 mod 2^32. */
	uint32_t checksum;

	/* These are only valid if MULTIBOOT_AOUT_KLUDGE is set. */
	uint32_t header_addr;
	uint32_t load_addr;
	uint32_t load_end_addr;
	uint32_t bss_end_addr;
	uint32_t entry_addr;

	/* These are only valid if MULTIBOOT_VIDEO_MODE is set. */
	uint32_t mode_type;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
} __packed;

#define MULTIBOOT_MEMORY_AVAILABLE			1U
#define MULTIBOOT_MEMORY_RESERVED			2U
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE		3U
#define MULTIBOOT_MEMORY_NVS				4U
#define MULTIBOOT_MEMORY_BADRAM				5U
struct multiboot_mmap {
	uint32_t	size;
	uint64_t	baseaddr;
	uint64_t	length;
	uint32_t	type;
} __packed;

struct multiboot_module {
	uint32_t	mm_mod_start;
	uint32_t	mm_mod_end;
	uint32_t	mm_string;
	uint32_t	mm_reserved;
} __packed;

struct multiboot_info {
	uint32_t	mi_flags;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MEMORY. */
	uint32_t	mi_mem_lower;
	uint32_t	mi_mem_upper;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_BOOT_DEVICE. */
	uint8_t		mi_boot_device_part3;
	uint8_t		mi_boot_device_part2;
	uint8_t		mi_boot_device_part1;
	uint8_t		mi_boot_device_drive;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_CMDLINE. */
	uint32_t	mi_cmdline;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MODS. */
	uint32_t	mi_mods_count;
	uint32_t	mi_mods_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_{AOUT,ELF}_SYMS. */
	uint32_t	mi_elfshdr_num;
	uint32_t	mi_elfshdr_size;
	uint32_t	mi_elfshdr_addr;
	uint32_t	mi_elfshdr_shndx;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MMAP. */
	uint32_t	mi_mmap_length;
	uint32_t	mi_mmap_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_DRIVES. */
	uint32_t	mi_drives_length;
	uint32_t	mi_drives_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_CONFIG_TABLE. */
	uint32_t	unused_mi_config_table;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_LOADER_NAME. */
	uint32_t	mi_loader_name;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_APM. */
	uint32_t	unused_mi_apm_table;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_VBE. */
	uint32_t	unused_mi_vbe_control_info;
	uint32_t	unused_mi_vbe_mode_info;
	uint16_t	unused_mi_vbe_mode;
	uint16_t	unused_mi_vbe_interface_seg;
	uint16_t	unused_mi_vbe_interface_off;
	uint16_t	unused_mi_vbe_interface_len;
} __packed;
#endif

#ifdef CONFIG_MULTIBOOT2
#define MULTIBOOT2_HEADER_ALIGN				8

#define MULTIBOOT2_HEADER_MAGIC				0xe85250d6U

/*  This should be in %eax. */
#define MULTIBOOT2_INFO_MAGIC				0x36d76289U

/*  Alignment of the multiboot info structure. */
#define MULTIBOOT2_INFO_ALIGN				0x00000008U

/*  Flags set in the 'flags' member of the multiboot header. */

#define MULTIBOOT2_TAG_ALIGN				8U
#define MULTIBOOT2_TAG_TYPE_END				0U
#define MULTIBOOT2_TAG_TYPE_CMDLINE			1U
#define MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME		2U
#define MULTIBOOT2_TAG_TYPE_MODULE			3U
#define MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO		4U
#define MULTIBOOT2_TAG_TYPE_BOOTDEV			5U
#define MULTIBOOT2_TAG_TYPE_MMAP			6U
#define MULTIBOOT2_TAG_TYPE_VBE				7U
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER			8U
#define MULTIBOOT2_TAG_TYPE_ELF_SECTIONS		9U
#define MULTIBOOT2_TAG_TYPE_APM				10U
#define MULTIBOOT2_TAG_TYPE_EFI32			11U
#define MULTIBOOT2_TAG_TYPE_EFI64			12U
#define MULTIBOOT2_TAG_TYPE_SMBIOS			13U
#define MULTIBOOT2_TAG_TYPE_ACPI_OLD			14U
#define MULTIBOOT2_TAG_TYPE_ACPI_NEW			15U
#define MULTIBOOT2_TAG_TYPE_NETWORK			16U
#define MULTIBOOT2_TAG_TYPE_EFI_MMAP			17U
#define MULTIBOOT2_TAG_TYPE_EFI_BS			18U
#define MULTIBOOT2_TAG_TYPE_EFI32_IH			19U
#define MULTIBOOT2_TAG_TYPE_EFI64_IH			20U
#define MULTIBOOT2_TAG_TYPE_LOAD_BASE_ADDR		21U

#define MULTIBOOT2_HEADER_TAG_END			0
#define MULTIBOOT2_HEADER_TAG_INFORMATION_REQUEST	1
#define MULTIBOOT2_HEADER_TAG_ADDRESS			2
#define MULTIBOOT2_HEADER_TAG_ENTRY_ADDRESS		3
#define MULTIBOOT2_HEADER_TAG_CONSOLE_FLAGS		4
#define MULTIBOOT2_HEADER_TAG_FRAMEBUFFER		5
#define MULTIBOOT2_HEADER_TAG_MODULE_ALIGN		6
#define MULTIBOOT2_HEADER_TAG_EFI_BS			7
#define MULTIBOOT2_HEADER_TAG_ENTRY_ADDRESS_EFI32	8
#define MULTIBOOT2_HEADER_TAG_ENTRY_ADDRESS_EFI64	9
#define MULTIBOOT2_HEADER_TAG_RELOCATABLE		10

#define MULTIBOOT2_ARCHITECTURE_I386			0
#define MULTIBOOT2_ARCHITECTURE_MIPS32			4

#ifndef ASSEMBLER

struct multiboot2_mmap_entry {
	uint64_t	addr;
	uint64_t	len;
	uint32_t	type;
	uint32_t	zero;
};

struct multiboot2_tag {
	uint32_t	type;
	uint32_t	size;
};

struct multiboot2_tag_string {
	uint32_t	type;
	uint32_t	size;
	char		string[0];
};

struct multiboot2_tag_module {
	uint32_t	type;
	uint32_t	size;
	uint32_t	mod_start;
	uint32_t	mod_end;
	char		cmdline[0];
};

struct multiboot2_tag_mmap {
	uint32_t	type;
	uint32_t	size;
	uint32_t	entry_size;
	uint32_t	entry_version;
	struct		multiboot2_mmap_entry entries[0];
};

struct multiboot2_tag_new_acpi {
	uint32_t	type;
	uint32_t	size;
	uint8_t		rsdp[0];
};

struct multiboot2_tag_efi64 {
	uint32_t	type;
	uint32_t	size;
	uint64_t	pointer;
};

struct multiboot2_tag_efi_mmap {
	uint32_t	type;
	uint32_t	size;
	uint32_t	descr_size;
	uint32_t	descr_vers;
	uint8_t		efi_mmap[0];
};
#endif

#endif /* CONFIG_MULTIBOOT2 */

#endif /*  MULTIBOOT2_STD_H */
