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

int fdt_get_phys_mem_region(const void *fdt, uint64_t *addr_out, uint64_t *size_out);
int fdt_get_rsvd_mem_regions(const void *fdt, struct mem_region *out_region, int *out_nr_region);
int fdt_add_rsvd_node(void *fdt, uint64_t addr, uint64_t size);

#endif /* FDT_API_H */
