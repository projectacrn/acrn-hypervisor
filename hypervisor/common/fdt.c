/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <libfdt.h>
#include <logmsg.h>
#include <memory.h>
#include <pgtable.h>
#include <fdt_api.h>

/* storage of raw fdt */
static uint8_t host_fdt_raw[MAX_FDT_SIZE];

void init_devtree(uint64_t fdt_paddr)
{
	void *fdt = hpa2hva_early(fdt_paddr);

	if (fdt_check_header(fdt) == 0) {
		if (fdt_totalsize(fdt) >= MAX_FDT_SIZE) {
			panic("FDT size 0x%x larger than configured maximum 0x%x",
					fdt_totalsize(fdt), MAX_FDT_SIZE);
		}

		/* copy raw data */
		fdt_move(fdt, host_fdt_raw, MAX_FDT_SIZE);
	} else {
		panic("Device tree not found or not supported", fdt_paddr);
	}
}

uint8_t *get_host_fdt(void)
{
	return (uint8_t *)host_fdt_raw;
}
