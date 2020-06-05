/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <types.h>
#include <pgtable.h>
#include <acpi.h>
#include <boot.h>
#include <vboot.h>
#include <direct_boot.h>
#include <deprivilege_boot.h>
#include <logmsg.h>

#define BOOTLOADER_NUM 5U
#define BOOTLOADER_NAME_SIZE 20U

struct vboot_bootloader_map {
	const char bootloader_name[BOOTLOADER_NAME_SIZE];
	enum vboot_mode mode;
};

static struct vboot_operations *vboot_ops;
static enum vboot_mode sos_boot_mode;

/**
 * @pre: this function is called during detect mode which is very early stage,
 * other exported interfaces should not be called beforehand.
 */
void init_vboot(void)
{
	struct acrn_multiboot_info *mbi = get_multiboot_info();
	uint32_t i;

	const struct vboot_bootloader_map vboot_bootloader_maps[BOOTLOADER_NUM] = {
		{"Slim BootLoader", DIRECT_BOOT_MODE},
		{"Intel IOTG/TSD ABL", DIRECT_BOOT_MODE},
		{"ACRN UEFI loader", DEPRI_BOOT_MODE},
		{"GRUB", DIRECT_BOOT_MODE},
		{"PXELINUX", DIRECT_BOOT_MODE},
	};

	for (i = 0U; i < BOOTLOADER_NUM; i++) {
		if (strncmp(mbi->mi_loader_name, vboot_bootloader_maps[i].bootloader_name,
			strnlen_s(vboot_bootloader_maps[i].bootloader_name, BOOTLOADER_NAME_SIZE)) == 0) {
			/* Only support two vboot mode */
			if (vboot_bootloader_maps[i].mode == DEPRI_BOOT_MODE) {
				vboot_ops = get_deprivilege_boot_ops();
				sos_boot_mode = DEPRI_BOOT_MODE;
			} else {
				vboot_ops = get_direct_boot_ops();
				sos_boot_mode = DIRECT_BOOT_MODE;
			}
			break;
		}
	}

	/*
	 * vboot_ops is mandatory and it will be initialized correctly.
	 * The vboot_ops->init is called to assure that the boot env is
	 * initialized before calling other vboot_ops interface.
	 */
	vboot_ops->init();
}

/* @pre: vboot_ops != NULL */
enum vboot_mode get_sos_boot_mode(void)
{
	return sos_boot_mode;
}

/* @pre: vboot_ops->get_ap_trampoline != NULL */
uint64_t get_ap_trampoline_buf(void)
{
	return vboot_ops->get_ap_trampoline();
}

/* @pre: vboot_ops->get_rsdp != NULL */
const void *get_rsdp_ptr(void)
{
	return vboot_ops->get_rsdp();
}

/* @pre: vboot_ops->init_irq != NULL */
void init_vboot_irq(void)
{
	return vboot_ops->init_irq();
}
