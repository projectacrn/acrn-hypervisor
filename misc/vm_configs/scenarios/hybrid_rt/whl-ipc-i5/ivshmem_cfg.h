/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef IVSHMEM_CFG_H
#define IVSHMEM_CFG_H

#include <ivshmem.h>
#include <pgtable.h>
#include <pci_devices.h>

/*
 * The IVSHMEM_SHM_SIZE is the sum of all memory regions.
 * The size range of each memory region is [2M, 1G) and is a power of 2.
 */
#define IVSHMEM_SHM_SIZE	0x200000UL
#define IVSHMEM_DEV_NUM		2UL

/* All user defined memory regions */

struct ivshmem_shm_region mem_regions[] = {
	{
		.name = IVSHMEM_SHM_REGION_0,
		.size = 0x200000UL,		/* 2M */
	},
};

#endif /* IVSHMEM_CFG_H */
