/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOOT_H_
#define BOOT_H_

#include <multiboot.h>
#include <e820.h>

#define MAX_BOOTARGS_SIZE		2048U
#define MAX_MODULE_COUNT		4U

struct acrn_multiboot_info {
	uint32_t		mi_flags;	/* the flags is back-compatible with multiboot1 */

	char			*mi_cmdline;
	char			*mi_loader_name;

	uint32_t		mi_mods_count;
	struct multiboot_module	mi_mods[MAX_MODULE_COUNT];

	uint32_t 		mi_drives_length;
	uint32_t		mi_drives_addr;

	uint32_t		mi_mmap_entries;
	struct multiboot_mmap	mi_mmap_entry[E820_MAX_ENTRIES];
};

/* boot_regs store the multiboot info magic and address */
extern uint32_t boot_regs[2];

static inline bool boot_from_multiboot1(void)
{
	return ((boot_regs[0] == MULTIBOOT_INFO_MAGIC) && (boot_regs[1] != 0U));
}

struct acrn_multiboot_info *get_multiboot_info(void);
int32_t sanitize_multiboot_info(void);
void parse_hv_cmdline(void);

#endif /* BOOT_H_ */
