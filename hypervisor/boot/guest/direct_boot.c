/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* this is for direct guest_boot method */

#include <types.h>
#include <e820.h>
#include <cpu.h>
#include <boot.h>
#include <direct_boot.h>
#include <mmu.h>

/* AP trampoline code buffer base address. */
static uint64_t ap_trampoline_buf;

static void init_direct_boot(void)
{
	ap_trampoline_buf = e820_alloc_memory(CONFIG_LOW_RAM_SIZE, MEM_1M);
}

/* @post: return != 0UL */
static uint64_t get_direct_boot_ap_trampoline(void)
{
	return ap_trampoline_buf;
}

static const void* get_direct_boot_rsdp(void)
{
#ifdef CONFIG_MULTIBOOT2
	struct acrn_multiboot_info *mbi = get_multiboot_info();

	return mbi->mi_acpi_rsdp_va;
#else
	return NULL;
#endif
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
