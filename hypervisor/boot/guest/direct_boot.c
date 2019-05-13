/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* this is for direct guest_boot method */

#include <types.h>
#include <e820.h>
#include <cpu.h>
#include <direct_boot.h>

static void init_direct_boot(void)
{
	/* nothing to do for now */
}

/* @post: return != 0UL */
static uint64_t get_direct_boot_ap_trampoline(void)
{
	return e820_alloc_low_memory(CONFIG_LOW_RAM_SIZE);
}

static void* get_direct_boot_rsdp(void)
{
	return NULL;
}

static void init_direct_boot_irq(void)
{
	CPU_IRQ_ENABLE();
}

static struct vboot_operations direct_boot_ops = {
	.init = init_direct_boot,
	.get_ap_trampoline = get_direct_boot_ap_trampoline,
	.get_rsdp = get_direct_boot_rsdp,
	.init_irq = init_direct_boot_irq,
};

struct vboot_operations* get_direct_boot_ops(void)
{
	return &direct_boot_ops;
}
