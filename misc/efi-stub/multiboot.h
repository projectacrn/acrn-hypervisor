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
	char *		mmo_string;
	uint32_t	mmo_reserved;
};

#endif /* !defined(_LOCORE) */

#endif /* _MULTIBOOT_H */
