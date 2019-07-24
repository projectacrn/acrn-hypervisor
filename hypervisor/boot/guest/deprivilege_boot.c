/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* this is for de-privilege guest vboot method */

#include <types.h>
#include <acrn_common.h>
#include <pgtable.h>
#include <logmsg.h>
#include <rtl.h>
#include <vlapic.h>
#include <lapic.h>
#include <per_cpu.h>
#include <multiboot.h>
#include <deprivilege_boot.h>

static struct depri_boot_context depri_boot_ctx;
static struct lapic_regs depri_boot_lapic_regs;

static void init_depri_boot(void)
{
	static bool depri_initialized = false;
	struct multiboot_info *mbi = NULL;

	if (!depri_initialized) {
		mbi = (struct multiboot_info *) hpa2hva(((uint64_t)(uint32_t)boot_regs[1]));
		if ((mbi == NULL) || ((mbi->mi_flags & MULTIBOOT_INFO_HAS_DRIVES) == 0U)) {
			pr_err("no multiboot drivers for depri_boot found");
		} else {
			(void)memcpy_s(&depri_boot_ctx, sizeof(struct depri_boot_context),
				hpa2hva((uint64_t)mbi->mi_drives_addr),
					sizeof(struct depri_boot_context));
			save_lapic(&depri_boot_lapic_regs);
		}
		depri_initialized = true;
	}
}

const struct depri_boot_context *get_depri_boot_ctx(void)
{
	return &depri_boot_ctx;
}

const struct lapic_regs *get_depri_boot_lapic_regs(void)
{
	return &depri_boot_lapic_regs;
}

static uint64_t get_depri_boot_ap_trampoline(void)
{
	return depri_boot_ctx.ap_trampoline_buf;
}

static void* get_depri_boot_rsdp(void)
{
	return hpa2hva((uint64_t)(depri_boot_ctx.rsdp));
}

static void init_depri_boot_irq(void)
{
	/* nothing to do for now */
}

static struct vboot_operations depri_boot_ops = {
	.init = init_depri_boot,
	.get_ap_trampoline = get_depri_boot_ap_trampoline,
	.get_rsdp = get_depri_boot_rsdp,
	.init_irq = init_depri_boot_irq,
};

struct vboot_operations* get_deprivilege_boot_ops(void)
{
	return &depri_boot_ops;
}
