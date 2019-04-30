/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* this is for both SBL and ABL platform */

#include <types.h>
#include <e820.h>
#include <cpu.h>
#include <direct_boot.h>

static void sbl_init(void)
{
	/* nothing to do for now */
}


/* @post: return != 0UL */
static uint64_t sbl_get_ap_trampoline(void)
{
	return e820_alloc_low_memory(CONFIG_LOW_RAM_SIZE);
}

static void* sbl_get_rsdp(void)
{
	return NULL;
}

static void sbl_init_irq(void)
{
	CPU_IRQ_ENABLE();
}

static struct firmware_operations firmware_sbl_ops = {
	.init = sbl_init,
	.get_ap_trampoline = sbl_get_ap_trampoline,
	.get_rsdp = sbl_get_rsdp,
	.init_irq = sbl_init_irq,
	.init_vm_boot_info = sbl_init_vm_boot_info,
};

struct firmware_operations* sbl_get_firmware_operations(void)
{
	return &firmware_sbl_ops;
}
