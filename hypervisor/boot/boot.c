/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/pgtable.h>
#include <boot.h>
#include <rtl.h>
#include <logmsg.h>

static struct acrn_boot_info acrn_bi = { 0U };

void init_acrn_boot_info(uint32_t *registers)
{
	(void)init_multiboot_info(registers);
	/* TODO: add more boot protocol support here */
}

int32_t sanitize_acrn_boot_info(struct acrn_boot_info *abi)
{
	int32_t abi_status = 0;

	if (abi->mods_count == 0U) {
		pr_err("no boot module info found");
		abi_status = -EINVAL;
	}

	if (abi->mmap_entries == 0U) {
		pr_err("no boot mmap info found");
		abi_status = -EINVAL;
	}

	if ((abi->mi_efi_info.efi_systab == 0U) && (abi->mi_efi_info.efi_systab_hi == 0U)) {
		pr_err("no uefi info found!");
	}

	if (abi->loader_name[0] == '\0') {
		pr_err("no bootloader name found!");
		abi_status = -EINVAL;
	} else {
		printf("%s Bootloader: %s\n", abi->protocol_name, abi->loader_name);
	}

	return abi_status;
}

/*
 * @post retval != NULL
 */
struct acrn_boot_info *get_acrn_boot_info(void)
{
	return &acrn_bi;
}
