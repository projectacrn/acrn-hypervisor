/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FDT_API_H
#define FDT_API_H
#include <types.h>
#include <libfdt.h>
#include <mmu.h>

#define MAX_FDT_SIZE (64 * MEM_1K)

void init_devtree(uint64_t fdt_paddr);
uint8_t *get_host_fdt(void);

#endif /* FDT_API_H */
