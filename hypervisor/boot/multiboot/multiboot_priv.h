/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MULTIBOOT_PRIV_H
#define MULTIBOOT_PRIV_H

#ifdef CONFIG_MULTIBOOT2
#include <multiboot2.h>

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
	uint32_t type;
	uint32_t size;
	uint64_t pointer;
};

struct multiboot2_tag_efi_mmap
{
	uint32_t type;
	uint32_t size;
	uint32_t descr_size;
	uint32_t descr_vers;
	uint8_t efi_mmap[0];
};

/*
 * @post boot_regs[1] stores the address pointer that point to a valid multiboot2 info
 */
static inline bool boot_from_multiboot2(uint32_t magic)
{
	/*
	 * Multiboot spec states that the Multiboot information structure may be placed
	 * anywhere in memory by the boot loader.
	 *
	 * Seems both SBL and GRUB won't place multiboot1 MBI structure at 0 address,
	 * but GRUB could place Multiboot2 MBI structure at 0 address until commit
	 * 0f3f5b7c13fa9b67 ("multiboot2: Set min address for mbi allocation to 0x1000")
	 * which dates on Dec 26 2019.
	 */
	return (magic == MULTIBOOT2_INFO_MAGIC);
}

int32_t multiboot2_to_acrn_mbi(struct acrn_multiboot_info *mbi, void *mb2_info, char *sig);
#endif

static inline bool boot_from_multiboot1(uint32_t magic, uint32_t info)
{
	return ((magic == MULTIBOOT_INFO_MAGIC) && (info != 0U));
}

#endif /* MULTIBOOT_PRIV_H */
