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
#include <vm_configurations.h>

#define MAX_BOOTARGS_SIZE		2048U
/* The modules in multiboot are: Pre-launched VM: kernel/ramdisk/acpi; SOS VM: kernel/ramdisk */
#define MAX_MODULE_NUM			(3U * PRE_VM_NUM + 2U * SOS_VM_NUM)

/* The vACPI module size is fixed to 1MB */
#define ACPI_MODULE_SIZE		MEM_1M

/* extended flags for acrn multiboot info from multiboot2  */
#define	MULTIBOOT_INFO_HAS_EFI_MMAP	0x00010000U
#define	MULTIBOOT_INFO_HAS_EFI64	0x00020000U

struct acrn_multiboot_info {
	uint32_t		mi_flags;	/* the flags is back-compatible with multiboot1 */

	const char		*mi_cmdline;
	const char		*mi_loader_name;

	uint32_t		mi_mods_count;
	const void		*mi_mods_va;
	struct multiboot_module	mi_mods[MAX_MODULE_NUM];

	uint32_t 		mi_drives_length;
	uint32_t		mi_drives_addr;

	uint32_t		mi_mmap_entries;
	const void		*mi_mmap_va;
	struct multiboot_mmap	mi_mmap_entry[E820_MAX_ENTRIES];

	const void		*mi_acpi_rsdp_va;
	struct efi_info		mi_efi_info;
};

/* boot_regs store the multiboot info magic and address */
extern uint32_t boot_regs[2];

extern char *efiloader_sig;

/*
 * The extern declaration for acrn_mbi is for cmdline.c use only, other functions should use
 * get_multiboot_info() API to access struct acrn_mbi because it has explict @post condition
 */
extern struct acrn_multiboot_info acrn_mbi;
struct acrn_multiboot_info *get_multiboot_info(void);
void init_acrn_multiboot_info(void);
int32_t sanitize_multiboot_info(void);
void parse_hv_cmdline(void);
const void* get_rsdp_ptr(void);

#endif /* BOOT_H_ */
