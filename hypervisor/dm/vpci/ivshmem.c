/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef CONFIG_IVSHMEM_ENABLED
#include <vm.h>
#include <mmu.h>
#include <ept.h>
#include <logmsg.h>
#include <ivshmem.h>
#include "vpci_priv.h"

/* IVSHMEM_SHM_SIZE is provided by offline tool */
static uint8_t ivshmem_base[IVSHMEM_SHM_SIZE] __aligned(PDE_SIZE);

void init_ivshmem_shared_memory()
{
	uint32_t i;
	uint64_t addr = hva2hpa(&ivshmem_base);

	for (i = 0U; i < ARRAY_SIZE(mem_regions); i++) {
		mem_regions[i].hpa = addr;
		addr += mem_regions[i].size;
	}
}

#endif
