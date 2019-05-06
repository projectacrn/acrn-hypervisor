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

static struct firmware_operations *firmware_ops;

/**
 * @pre: this function is called during detect mode which is very early stage,
 * other exported interfaces should not be called beforehand.
 */
void init_firmware_operations(void)
{

	struct multiboot_info *mbi;
	uint32_t i;

	const struct firmware_candidates fw_candidates[NUM_FIRMWARE_SUPPORTING] = {
		{"Slim BootLoader", 15U, sbl_get_firmware_operations},
		{"Intel IOTG/TSD ABL", 18U, sbl_get_firmware_operations},
		{"ACRN UEFI loader", 16U, uefi_get_firmware_operations},
		{"GRUB", 4U, sbl_get_firmware_operations},
	};

	mbi = (struct multiboot_info *)hpa2hva((uint64_t)boot_regs[1]);
	for (i = 0U; i < NUM_FIRMWARE_SUPPORTING; i++) {
		if (strncmp(hpa2hva(mbi->mi_loader_name), fw_candidates[i].name, fw_candidates[i].name_sz) == 0) {
			firmware_ops = fw_candidates[i].ops();
			break;
		}
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
