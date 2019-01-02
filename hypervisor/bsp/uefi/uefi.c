/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <multiboot.h>
#include <boot_context.h>
#include <vm0_boot.h>

#ifdef CONFIG_EFI_STUB

static struct efi_context *efi_ctx = NULL;
static struct lapic_regs uefi_lapic_regs;
static int32_t efi_initialized;

static void efi_init(void)
{
	struct multiboot_info *mbi = NULL;

	if (boot_regs[0] != MULTIBOOT_INFO_MAGIC) {
		pr_err("no multiboot info found");
	} else {

		mbi = (struct multiboot_info *) hpa2hva(((uint64_t)(uint32_t)boot_regs[1]));
		if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_DRIVES) == 0U) {
			pr_err("no multiboot drivers for uefi found");
		} else {

			efi_ctx = (struct efi_context *)hpa2hva((uint64_t)mbi->mi_drives_addr);
			if (efi_ctx == NULL) {
				pr_err("no uefi context found");
			} else {

				save_lapic(&uefi_lapic_regs);
				efi_initialized = 1;
			}
		}
	}
}

void *get_rsdp_from_uefi(void)
{
	if (!efi_initialized) {
		efi_init();
	}

	return hpa2hva((uint64_t)efi_ctx->rsdp);
}

void *get_ap_trampoline_buf(void)
{
	return efi_ctx->ap_trampoline_buf;
}

const struct efi_context *get_efi_ctx(void)
{
	return efi_ctx;
}

const struct lapic_regs *get_efi_lapic_regs(void)
{
	return &uefi_lapic_regs;
}

#endif

void init_bsp(void)
{
#ifndef CONFIG_CONSTANT_ACPI
	acpi_fixup();
#endif
	parse_hv_cmdline();

#ifdef CONFIG_EFI_STUB
	if (!efi_initialized)
		efi_init();
#endif
}
