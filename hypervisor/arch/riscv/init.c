/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <types.h>

/* C entry point for boot CPU */
void init_primary_pcpu(uint64_t hart_id, uint64_t fdt_paddr)
{
	(void)hart_id;
	(void)fdt_paddr;

	while (1) {
		asm volatile("wfi" : : : "memory");
	}
}

/* C entry point for AP */
void init_secondary_pcpu(uint64_t hart_id)
{
	(void)hart_id;

	/* to be implemented */
}
