/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <multiboot.h>
#include <vm.h>
#include <types.h>
#include <pgtable.h>
#include <acpi.h>
#include <vboot.h>
#include <direct_boot.h>
#include <deprivilege_boot.h>

static struct vboot_operations *vboot_ops;

/**
 * @pre: this function is called during detect mode which is very early stage,
 * other exported interfaces should not be called beforehand.
 */
void init_vboot_operations(void)
{

	struct multiboot_info *mbi;
	uint32_t i;

	const struct vboot_candidates vboot_candidates[NUM_VBOOT_SUPPORTING] = {
		{"Slim BootLoader", 15U, get_direct_boot_ops},
		{"Intel IOTG/TSD ABL", 18U, get_direct_boot_ops},
		{"ACRN UEFI loader", 16U, get_deprivilege_boot_ops},
		{"GRUB", 4U, get_direct_boot_ops},
	};

	mbi = (struct multiboot_info *)hpa2hva((uint64_t)boot_regs[1]);
	for (i = 0U; i < NUM_VBOOT_SUPPORTING; i++) {
		if (strncmp(hpa2hva(mbi->mi_loader_name), vboot_candidates[i].name,
			vboot_candidates[i].name_sz) == 0) {
			vboot_ops = vboot_candidates[i].ops();
			break;
		}
	}
}

/* @pre: vboot_ops->init != NULL */
void init_vboot(void)
{
#ifndef CONFIG_CONSTANT_ACPI
	acpi_fixup();
#endif
	vboot_ops->init();
}

/* @pre: vboot_ops->get_ap_trampoline != NULL */
uint64_t get_ap_trampoline_buf(void)
{
	return vboot_ops->get_ap_trampoline();
}

/* @pre: vboot_ops->get_rsdp != NULL */
void *get_rsdp_ptr(void)
{
	return vboot_ops->get_rsdp();
}

/* @pre: vboot_ops->init_irq != NULL */
void init_vboot_irq(void)
{
	return vboot_ops->init_irq();
}

/* @pre: vboot_ops->init_vboot_info != NULL */
int32_t init_vm_boot_info(struct acrn_vm *vm)
{
	return vboot_ops->init_vboot_info(vm);
}
