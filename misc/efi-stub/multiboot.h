/*	[ORIGIN: src/sys/arch/i386/include/...				*/
/*	$NetBSD: multiboot.h,v 1.8 2009/02/22 18:05:42 ahoka Exp $	*/

/*-
 * Copyright (c) 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * multiboot.h
 */

#ifndef _MULTIBOOT_H
#define _MULTIBOOT_H

#include <stdint.h>
#include <types.h>

struct multiboot_info;
extern struct multiboot_info mbi;

/*
 * Multiboot header structure.
 */
#define MULTIBOOT_HEADER_MAGIC		0x1BADB002
#define MULTIBOOT_HEADER_MODS_ALIGNED	0x00000001
#define MULTIBOOT_HEADER_WANT_MEMORY	0x00000002
#define MULTIBOOT_HEADER_HAS_VBE	0x00000004
#define MULTIBOOT_HEADER_HAS_ADDR	0x00010000

#define MULTIBOOT_HEADER_ALIGN		4
#define MULTIBOOT_SEARCH		8192

#if !defined(_LOCORE)
struct multiboot_header {
	uint32_t	mh_magic;
	uint32_t	mh_flags;
	uint32_t	mh_checksum;

	/* Valid if mh_flags sets MULTIBOOT_HEADER_HAS_ADDR. */
	uint32_t		mh_header_addr;
	uint32_t		mh_load_addr;
	uint32_t		mh_load_end_addr;
	uint32_t		mh_bss_end_addr;
	uint32_t		mh_entry_addr;

	/* Valid if mh_flags sets MULTIBOOT_HEADER_HAS_VBE. */
	uint32_t	mh_mode_type;
	uint32_t	mh_width;
	uint32_t	mh_height;
	uint32_t	mh_depth;
};
#endif /* !defined(_LOCORE) */

/*
 * Symbols defined in locore.S.
 */
extern struct multiboot_header *Multiboot_Header;

/*
 * Multiboot information structure.
 */
#define MULTIBOOT_INFO_MAGIC		0x2BADB002U
#define MULTIBOOT_INFO_HAS_MEMORY	0x00000001U
#define MULTIBOOT_INFO_HAS_BOOT_DEVICE	0x00000002U
#define MULTIBOOT_INFO_HAS_CMDLINE	0x00000004U
#define MULTIBOOT_INFO_HAS_MODS		0x00000008U
#define MULTIBOOT_INFO_HAS_AOUT_SYMS	0x00000010U
#define MULTIBOOT_INFO_HAS_ELF_SYMS	0x00000020U
#define MULTIBOOT_INFO_HAS_MMAP		0x00000040U
#define MULTIBOOT_INFO_HAS_DRIVES	0x00000080U
#define MULTIBOOT_INFO_HAS_CONFIG_TABLE	0x00000100U
#define MULTIBOOT_INFO_HAS_LOADER_NAME	0x00000200U
#define MULTIBOOT_INFO_HAS_APM_TABLE	0x00000400U
#define MULTIBOOT_INFO_HAS_VBE		0x00000800U

#if !defined(_LOCORE)
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
	uint32_t		mi_mods_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_{AOUT,ELF}_SYMS. */
	uint32_t	mi_elfshdr_num;
	uint32_t	mi_elfshdr_size;
	uint32_t		mi_elfshdr_addr;
	uint32_t	mi_elfshdr_shndx;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MMAP. */
	uint32_t	mi_mmap_length;
	uint32_t	mi_mmap_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_DRIVES. */
	uint32_t	mi_drives_length;
	uint32_t		mi_drives_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_CONFIG_TABLE. */
	uint32_t		unused_mi_config_table;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_LOADER_NAME. */
	uint32_t		mi_loader_name;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_APM. */
	uint32_t		unused_mi_apm_table;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_VBE. */
	uint32_t		unused_mi_vbe_control_info;
	uint32_t 	unused_mi_vbe_mode_info;
	uint32_t		unused_mi_vbe_interface_seg;
	uint32_t		unused_mi_vbe_interface_off;
	uint32_t	unused_mi_vbe_interface_len;
}__aligned(8);


/*
 * Memory mapping.  This describes an entry in the memory mappings table
 * as pointed to by mi_mmap_addr.
 *
 * Be aware that mm_size specifies the size of all other fields *except*
 * for mm_size.  In order to jump between two different entries, you
 * have to count mm_size + 4 bytes.
 */
struct __attribute__((packed)) multiboot_mmap {
	uint32_t	mm_size;
	uint64_t	mm_base_addr;
	uint64_t	mm_length;
	uint32_t	mm_type;
};

/*
 * Modules. This describes an entry in the modules table as pointed
 * to by mi_mods_addr.
 */

struct multiboot_module {
	uint32_t	mmo_start;
	uint32_t	mmo_end;
	uint32_t	mmo_string;
	uint32_t	mmo_reserved;
} __packed;

#endif /* !defined(_LOCORE) */

struct multiboot2_header
{
  uint32_t magic;
  uint32_t architecture;
  uint32_t header_length;
  uint32_t checksum;
};

#define MULTIBOOT2_SEARCH                        32768

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
#define MULTIBOOT2_HEADER_TAG_OPTIONAL			1

#define MULTIBOOT2_ARCHITECTURE_I386			0

#ifndef ASSEMBLER

struct multiboot2_header_tag
{
	uint16_t type;
	uint16_t flags;
	uint32_t size;
};

struct multiboot2_header_tag_information_request
{
	uint16_t type;
	uint16_t flags;
	uint32_t size;
	uint32_t requests[0];
};

struct multiboot2_header_tag_address
{
	uint16_t type;
	uint16_t flags;
	uint32_t size;
	uint32_t header_addr;
	uint32_t load_addr;
	uint32_t load_end_addr;
	uint32_t bss_end_addr;
};

struct multiboot2_header_tag_entry_address
{
	uint16_t type;
	uint16_t flags;
	uint32_t size;
	uint32_t entry_addr;
};

struct multiboot2_header_tag_console_flags
{
	uint16_t type;
	uint16_t flags;
	uint32_t size;
	uint32_t console_flags;
};

struct multiboot2_header_tag_framebuffer
{
	uint16_t type;
	uint16_t flags;
	uint32_t size;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
};

struct multiboot2_header_tag_module_align
{
	uint16_t type;
	uint16_t flags;
	uint32_t size;
};

struct multiboot2_header_tag_relocatable
{
	uint16_t type;
	uint16_t flags;
	uint32_t size;
	uint32_t min_addr;
	uint32_t max_addr;
	uint32_t align;
	uint32_t preference;
};

struct multiboot2_mmap_entry
{
	uint64_t addr;
	uint64_t len;
	uint32_t type;
	uint32_t zero;
};

struct multiboot2_tag
{
	uint32_t type;
	uint32_t size;
};

struct multiboot2_tag_string
{
	uint32_t type;
	uint32_t size;
	char string[0];
};

struct multiboot2_tag_module
{
	uint32_t type;
	uint32_t size;
	uint32_t mod_start;
	uint32_t mod_end;
	char cmdline[0];
};

struct multiboot2_tag_mmap
{
	uint32_t type;
	uint32_t size;
	uint32_t entry_size;
	uint32_t entry_version;
	struct multiboot2_mmap_entry entries[0];
};

struct multiboot2_tag_new_acpi
{
	uint32_t type;
	uint32_t size;
	uint8_t rsdp[0];
};

struct multiboot2_tag_efi64
{
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

struct hv_mb2header_tag_list {
	struct multiboot2_header_tag_information_request *info_req;
	struct multiboot2_header_tag_address *addr;
	struct multiboot2_header_tag_entry_address *entry;
	struct multiboot2_header_tag_console_flags *console_flags;
	struct multiboot2_header_tag_framebuffer *frbuf;
	struct multiboot2_header_tag_module_align *modalign;
	struct multiboot2_header_tag_relocatable *reloc;
};

const struct multiboot_header *find_mb1header(const UINT8 *buffer, uint64_t len);
const struct multiboot2_header *find_mb2header(const UINT8 *buffer, uint64_t len);
int parse_mb2header(const struct multiboot2_header *header, struct hv_mb2header_tag_list *hv_tags);

#endif /* _MULTIBOOT_H */
