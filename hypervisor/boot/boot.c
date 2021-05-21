/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/pgtable.h>
#include <multiboot.h>
#include <rtl.h>
#include <logmsg.h>
#include "multiboot/multiboot_priv.h"

static struct acrn_boot_info acrn_bi = { 0U };
static char boot_protocol_name[16U] = { 0 };

void init_acrn_boot_info(uint32_t *registers)
{
	if (init_multiboot_info(registers) == 0) {
		strncpy_s(boot_protocol_name, 16U, "Multiboot", 16U);
#ifdef CONFIG_MULTIBOOT2
	} else if (init_multiboot2_info(registers) == 0) {
		strncpy_s(boot_protocol_name, 16U, "Multiboot2", 16U);
#endif
	}
}

int32_t sanitize_acrn_boot_info(struct acrn_boot_info *abi)
{
	int32_t abi_status = 0;

	if (abi->mi_mmap_entries != 0U) {
		abi->mi_flags |= MULTIBOOT_INFO_HAS_MMAP;
	} else {
		abi->mi_flags &= ~MULTIBOOT_INFO_HAS_MMAP;
	}

	if (abi->mi_mods_count != 0U) {
		abi->mi_flags |= MULTIBOOT_INFO_HAS_MODS;
	} else {
		abi->mi_flags &= ~MULTIBOOT_INFO_HAS_MODS;
	}

	if ((abi->mi_flags & MULTIBOOT_INFO_HAS_MODS) == 0U) {
		pr_err("no multiboot module info found");
		abi_status = -EINVAL;
	}

	if ((abi->mi_flags & MULTIBOOT_INFO_HAS_MMAP) == 0U) {
		pr_err("wrong multiboot flags: 0x%08x", abi->mi_flags);
		abi_status = -EINVAL;
	}

#ifdef CONFIG_MULTIBOOT2
	if ((abi->mi_flags & (MULTIBOOT_INFO_HAS_EFI64 | MULTIBOOT_INFO_HAS_EFI_MMAP)) == 0U) {
		pr_err("no multiboot2 uefi info found!");
	}
#endif

	if (abi->mi_loader_name[0] == '\0') {
		pr_err("no bootloader name found!");
		abi_status = -EINVAL;
	} else {
		printf("%s Bootloader: %s\n", boot_protocol_name, abi->mi_loader_name);
	}

	return abi_status;
}

/*
 * @post retval != NULL
 * @post retval->mi_flags & MULTIBOOT_INFO_HAS_MMAP != 0U
 * @post (retval->mi_mmap_entries > 0U) && (retval->mi_mmap_entries <= MAX_MMAP_ENTRIES)
 */
struct acrn_boot_info *get_acrn_boot_info(void)
{
	return &acrn_bi;
}
