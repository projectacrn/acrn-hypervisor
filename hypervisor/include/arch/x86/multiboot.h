/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#define MULTIBOOT_INFO_MAGIC		    	0x2BADB002
#define MULTIBOOT_INFO_HAS_CMDLINE		0x00000004
#define MULTIBOOT_INFO_HAS_MODS			0x00000008
#define MULTIBOOT_INFO_HAS_DRIVES	0x00000080

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
int init_vm0_boot_info(struct vm *vm);

#endif
