/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <e820.h>

void    init_bsp(void)
{
#ifndef CONFIG_CONSTANT_ACPI
	acpi_fixup();
#endif
}

uint64_t bsp_get_ap_trampoline(void)
{
	return e820_alloc_low_memory(CONFIG_LOW_RAM_SIZE);
}

void *bsp_get_rsdp(void)
{
	return NULL;
}

void bsp_init_irq(void)
{
	CPU_IRQ_ENABLE();
}
