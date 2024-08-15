/*
 * Copyright (C) 2020-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MULTIBOOT_PRIV_H
#define MULTIBOOT_PRIV_H

#include <multiboot_std.h>

#ifdef CONFIG_MULTIBOOT2
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

int32_t multiboot2_to_acrn_bi(struct acrn_boot_info *abi, void *mb2_info);
#endif

static inline bool boot_from_multiboot(uint32_t magic, uint32_t info)
{
	return ((magic == MULTIBOOT_INFO_MAGIC) && (info != 0U));
}

#endif /* MULTIBOOT_PRIV_H */
