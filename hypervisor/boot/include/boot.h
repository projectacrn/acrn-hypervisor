/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOOT_H_
#define BOOT_H_

#include <multiboot.h>
#ifdef CONFIG_MULTIBOOT2
#include <multiboot2.h>
#endif
#include <e820.h>
#include <zeropage.h>

#define MAX_BOOTARGS_SIZE		2048U
#define MAX_MODULE_COUNT		4U

/* extended flags for acrn multiboot info from multiboot2  */
#define	MULTIBOOT_INFO_HAS_EFI_MMAP	0x00010000U
#define	MULTIBOOT_INFO_HAS_EFI64	0x00020000U

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

	void			*mi_acpi_rsdp;
	struct efi_info		mi_efi_info;
};

/* boot_regs store the multiboot info magic and address */
extern uint32_t boot_regs[2];

extern char *efiloader_sig;

static inline bool boot_from_multiboot1(void)
{
	return ((boot_regs[0] == MULTIBOOT_INFO_MAGIC) && (boot_regs[1] != 0U));
}

#ifdef CONFIG_MULTIBOOT2
static inline bool boot_from_multiboot2(void)
{
	return ((boot_regs[0] == MULTIBOOT2_INFO_MAGIC) && (boot_regs[1] != 0U));
}

int32_t multiboot2_to_acrn_mbi(struct acrn_multiboot_info *mbi, void *mb2_info);
#endif

struct acrn_multiboot_info *get_multiboot_info(void);
int32_t sanitize_multiboot_info(void);
void parse_hv_cmdline(void);

#endif /* BOOT_H_ */
