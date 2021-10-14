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

/**
 * @pre (p_start != NULL) && (p_end != NULL)
 */
void get_boot_mods_range(uint64_t *p_start, uint64_t *p_end)
{
	uint32_t i;
	uint64_t start = ~0UL, end = 0UL;
	struct acrn_boot_info *abi = get_acrn_boot_info();

	for (i = 0; i < abi->mods_count; i++) {
		if (hva2hpa(abi->mods[i].start) < start) {
			start = hva2hpa(abi->mods[i].start);
		}
		if (hva2hpa(abi->mods[i].start + abi->mods[i].size) > end) {
			end = hva2hpa(abi->mods[i].start + abi->mods[i].size);
		}
	}
	*p_start = start;
	*p_end = end;
}

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

	printf("%s environment detected.\n", boot_from_uefi(abi) ? "UEFI" : "Non-UEFI");
	if (boot_from_uefi(abi) && ((abi->uefi_info.memmap == 0U) || (abi->uefi_info.memmap_hi != 0U))) {
		pr_err("no efi memmap found below 4GB space!");
		abi_status = -EINVAL;
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
