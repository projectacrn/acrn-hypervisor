/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <multiboot.h>
#include <vm.h>
#include <types.h>
#include <pgtable.h>
#include <firmware.h>
#include <firmware_sbl.h>
#include <firmware_uefi.h>
#include "platform_acpi_info.h"

static struct firmware_operations *firmware_ops;

static bool is_firmware_sbl(void)
{
	bool ret = false;
	struct multiboot_info *mbi = NULL;
	size_t i;
	const char *sbl_candidates[] = {
		"Slim BootLoader",
		"Intel IOTG/TSD ABL",
	};

	mbi = (struct multiboot_info *)hpa2hva((uint64_t)boot_regs[1]);
	if (mbi != NULL) {
		if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_LOADER_NAME) != 0U) {
			for (i = 0; i < sizeof(sbl_candidates); i++) {
				if (strcmp(hpa2hva(mbi->mi_loader_name), sbl_candidates[i]) == 0) {
					ret = true;
					break;
				}
			}
		}
	}

	return ret;
}

/**
 * @pre: this function is called during detect mode which is very early stage,
 * other exported interfaces should not be called beforehand.
 */
void init_firmware_operations(void)
{
	if (is_firmware_sbl()) {
		firmware_ops = sbl_get_firmware_operations();
	} else {
		firmware_ops = uefi_get_firmware_operations();
	}
}


/* @pre: firmware_ops->init != NULL */
void init_firmware(void)
{
#ifndef CONFIG_CONSTANT_ACPI
	acpi_fixup();
#endif
	firmware_ops->init();
}

/* @pre: firmware_ops->get_ap_trampoline != NULL */
uint64_t firmware_get_ap_trampoline(void)
{
	return firmware_ops->get_ap_trampoline();
}

/* @pre: firmware_ops->get_rsdp != NULL */
void *firmware_get_rsdp(void)
{
	return firmware_ops->get_rsdp();
}

/* @pre: firmware_ops->init_irq != NULL */
void firmware_init_irq(void)
{
	return firmware_ops->init_irq();
}

/* @pre: firmware_ops->init_vm_boot_info != NULL */
int32_t firmware_init_vm_boot_info(struct acrn_vm *vm)
{
	return firmware_ops->init_vm_boot_info(vm);
}
