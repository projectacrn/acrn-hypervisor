/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IVSHMEM_CFG_H
#define IVSHMEM_CFG_H

#include <ivshmem.h>
#include <x86/pgtable.h>
#define IVSHMEM_SHM_REGION_0 "hv:/shm_region_0"

/* The IVSHMEM_SHM_SIZE is the sum of all memory regions. The size range of each memory region is [2MB, 512MB] and is a
 * power of 2. */
#define IVSHMEM_SHM_SIZE 0x200000UL
#define IVSHMEM_DEV_NUM 2UL
/* All user defined memory regions */
#define IVSHMEM_SHM_REGIONS                                                                                            \
	{                                                                                                              \
		.name = IVSHMEM_SHM_REGION_0,                                                                          \
		.size = 0x200000UL,                                                                                    \
	},

#endif /* IVSHMEM_CFG_H */
