/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <libfdt.h>
#include <logmsg.h>
#include <memory.h>
#include <pgtable.h>
#include <fdt_api.h>
#include <mmu.h>
#include <sprintf.h>

/* storage of raw fdt */
static uint8_t host_fdt_raw[MAX_FDT_SIZE] __aligned(8);

/**
 * Caller needs to make sure that the length >= 16
 */
static void fdt_read_reg_property(const struct fdt_property *reg_prop,
		uint64_t *addr_out, uint64_t *size_out)
{
	fdt32_t *addr_raw_lo, *addr_raw_hi, *size_raw_lo, *size_raw_hi;

	addr_raw_hi = (fdt32_t *)&reg_prop->data[0];
	addr_raw_lo = (fdt32_t *)&reg_prop->data[4];
	size_raw_hi = (fdt32_t *)&reg_prop->data[8];
	size_raw_lo = (fdt32_t *)&reg_prop->data[12];
	*addr_out = (((uint64_t)fdt32_to_cpu(*addr_raw_hi) << 32) \
			| fdt32_to_cpu(*addr_raw_lo));
	*size_out = (((uint64_t)fdt32_to_cpu(*size_raw_hi) << 32) \
			| fdt32_to_cpu(*size_raw_lo));
}

/**
 * @pre addr_out != NULL
 * @pre size_out != NULL
 */
int fdt_get_phys_mem_region(const void *fdt, uint64_t *addr_out, uint64_t *size_out)
{
	int ret = 0;
	const struct fdt_property *reg_prop;
	int mem_off, len;

	mem_off = fdt_path_offset(fdt, "/memory");
	if (mem_off > 0) {
		/*
		 * TODO: For now this API has two assumptions:
		 * 1, address and cell sizes are 2 (64bit)
		 * 2, there is only 1 memory node
		 */
		reg_prop = fdt_get_property(fdt, mem_off, "reg", &len);
		/* minimal 16 bytes for 64bit addr + size */
		if ((reg_prop != NULL) && (len >= 16)) {
			fdt_read_reg_property(reg_prop, addr_out, size_out);
		} else {
			ret = -EINVAL;
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

/**
 * Return a list of reserved memory ranges.
 * Does not guarantee order or overlapping.
 */
int fdt_get_rsvd_mem_regions(const void *fdt, struct mem_region *out_regions, int *out_nr_region)
{
	int node, nr_region = 0, ret = 0, mem_off, i, len;
	const struct fdt_property *reg_prop;

	/* Check dt struct rsvd memory */
	mem_off = fdt_path_offset(fdt, "/reserved-memory");
	if (mem_off > 0) {
		/* TODO: Check address and size cells. Both of them need to be 2 (64bit) */
		fdt_for_each_subnode(node, fdt, mem_off) {
			reg_prop = fdt_get_property(fdt, node, "reg", &len);
			if ((reg_prop != NULL) && (len >= 16)) {
				fdt_read_reg_property(reg_prop,
						&(out_regions[nr_region].addr), &(out_regions[nr_region].size));
				nr_region++;
			} else {
				ret = -EINVAL;
			}
		}
	}

	/* Check rsvd mem block */
	for (i = 0; i < fdt_num_mem_rsv(fdt); i++) {
		ret = fdt_get_mem_rsv(fdt, i,
				&(out_regions[nr_region].addr), &(out_regions[nr_region].size));
		if (ret == 0) {
			nr_region++;
		}
	}

	*out_nr_region = nr_region;

	return ret;
}

/*
 * Assume addr and cell sizes are always 2
 */
int fdt_add_rsvd_node(void *fdt, uint64_t addr, uint64_t size)
{
	int mem_off, index = 0, subnode, ret = 0;
	uint32_t addr_hi, addr_lo, size_hi, size_lo;
	fdt32_t reg[4];
	char name[64];

	addr_hi = (uint32_t)(addr >> 32);
	addr_lo = (uint32_t)(addr & 0xffffffff);
	size_hi = (uint32_t)(size >> 32);
	size_lo = (uint32_t)(size & 0xffffffff);

	reg[0] = cpu_to_fdt32(addr_hi);
	reg[1] = cpu_to_fdt32(addr_lo);
	reg[2] = cpu_to_fdt32(size_hi);
	reg[3] = cpu_to_fdt32(size_lo);

	mem_off = fdt_path_offset(fdt, "/reserved-memory");
	if (mem_off < 0) {
		/* no reserved memory yet, create one under root */
		ret = fdt_add_subnode(fdt, 0, "reserved-memory");
		if (ret == 0) {
			mem_off = ret;
		}

		ret = fdt_setprop_empty(fdt, mem_off, "ranges");

		/* ACRN supports 64-bit only */
		if (ret == 0) {
			ret = fdt_setprop_u32(fdt, mem_off, "#size-cells", 2);
		}

		if (ret == 0) {
			ret = fdt_setprop_u32(fdt, mem_off, "#address-cells", 2);
		}
	}

	if ((mem_off > 0) && (ret == 0)) {
		fdt_for_each_subnode(subnode, fdt, mem_off) {
			index++;
		}

		if (addr < MEM_4G) {
			snprintf(name, 64, "mmode_resv%d@%x", index, addr_lo);
		} else {
			snprintf(name, 64, "mmode_resv%d@%x,%x", index, addr_hi, addr_lo);
		}

		subnode = fdt_add_subnode(fdt, mem_off, name);
		if (subnode > 0) {
			ret = fdt_setprop_empty(fdt, subnode, "no-map");
			if (ret == 0) {
				fdt_setprop(fdt, subnode, "reg", reg, 4 * sizeof(fdt32_t));
			}
		} else {
			ret = subnode;
		}
	}

	return ret;
}

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
