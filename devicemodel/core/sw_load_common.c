/*-
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "vmmapi.h"
#include "sw_load.h"
#include "dm.h"
#include "pci_core.h"
#include "rtct.h"

int with_bootargs;
static char bootargs[BOOT_ARG_LEN];

/*
 * Default e820 mem map:
 *
 * there is reserved memory hole for PCI hole and APIC etc
 * so the memory layout could be separated into lowmem & highmem.
 * - if request memory size <= ctx->lowmem_limit, then there is only
 *   map[0]:0~ctx->lowmem for RAM
 *   ctx->lowmem = request_memory_size
 * - if request memory size > ctx->lowmem_limit, then there are
 *   map[0]:0~ctx->lowmem_limit & map[2]:4G~ctx->highmem for RAM
 *   ctx->highmem = request_memory_size - ctx->lowmem_limit
 *
 *             Begin    Limit        Type            Length
 * 0:              0 -  0xA0000      RAM             0xA0000
 * 1:       0x100000 -  lowmem part1 RAM             0x0
 * 2:   gpu_rsvd_bot -  gpu_rsvd_top (reserved)      0x4004000
 * 3:   lowmem part2 -  0x7f800000   (reserved)      0x0
 * 4:   SW SRAM_bot  -  0x80000000   (reserved)      SOFTWARE_SRAM_MAX_SIZE
 * 5:     0xDB000000 -  0xDF000000   (reserved)      64MB
 * 6:     0xDF000000 -  0xE0000000   (reserved)      16MB
 * 7:     0xE0000000 -  0x100000000  MCFG, MMIO      512MB
 * 8:    0x140000000 -  highmem      RAM             highmem - 5GB
 *
 * FIXME: Do we need to reserve DSM and OPREGION for GVTD here.
 */

#define GPU_DSM_OPREGION_BASE_GPA 0x3B800000
#define GPU_DSM_OPREGION_SIZE  0x4004000

const struct e820_entry e820_default_entries[NUM_E820_ENTRIES] = {
	{	/* 0 to video memory */
		.baseaddr = 0x00000000,
		.length   = 0xA0000,
		.type     = E820_TYPE_RAM
	},

	{	/* 1MB to lowmem part1 */
		.baseaddr = 1 * MB,
		.length   = 0x0,
		.type     = E820_TYPE_RAM
	},

	{	/* TGL GPU DSM & OpRegion area */
		.baseaddr = GPU_DSM_OPREGION_BASE_GPA,
		.length   = GPU_DSM_OPREGION_SIZE,
		.type     = E820_TYPE_RESERVED
	},

	{	/* lowmem part2 to lowmem_limit */
		.baseaddr = GPU_DSM_OPREGION_BASE_GPA + GPU_DSM_OPREGION_SIZE,
		.length   = 0x0,
		.type     = E820_TYPE_RESERVED
	},

	/*
	 * Software SRAM area: base: 0x7f800000, size: 0x800000
	 * In native, the Software SRAM region should be part of DRAM memory.
	 * But one fixed Software SRAM gpa is friendly for virtualization due
	 * to decoupled with various guest memory size.
	 */
	{
		.baseaddr = SOFTWARE_SRAM_BASE_GPA,
		.length   = SOFTWARE_SRAM_MAX_SIZE,
		.type	  = E820_TYPE_RESERVED
	},

	{	/* ECFG_BASE to 4GB */
		.baseaddr = PCI_EMUL_ECFG_BASE,
		.length   = (4 * GB) - PCI_EMUL_ECFG_BASE,
		.type     = E820_TYPE_RESERVED
	},

	{	/* 5GB to highmem */
		.baseaddr = PCI_EMUL_MEMLIMIT64,
		.length   = 0x0,
		.type     = E820_TYPE_RESERVED
	},
};

int
acrn_parse_bootargs(char *arg)
{
	size_t len = strnlen(arg, BOOT_ARG_LEN);

	if (len < BOOT_ARG_LEN) {
		strncpy(bootargs, arg, len + 1);
		with_bootargs = 1;
		pr_notice("SW_LOAD: get bootargs %s\n", bootargs);
		return 0;
	}
	return -1;
}

char*
get_bootargs(void)
{
	return bootargs;
}

int
check_image(char *path, size_t size_limit, size_t *size)
{
	FILE *fp;
	long len;

	fp = fopen(path, "r");

	if (fp == NULL) {
		pr_err("SW_LOAD ERR: image file failed to open\n");
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);

	if (len == 0 || (size_limit && len > size_limit)) {
		pr_err("SW_LOAD ERR: file is %s\n",
			len ? "too large" : "empty");
		fclose(fp);
		return -1;
	}

	fclose(fp);
	*size = len;
	return 0;
}

/* Assumption:
 * the range [start, start + size] belongs to one entry of e820 table
 */
int
add_e820_entry(struct e820_entry *e820, int len, uint64_t start,
	uint64_t size, uint32_t type)
{
	int i, length = len;
	uint64_t e_s, e_e;

	for (i = 0; i < len; i++) {
		e_s = e820[i].baseaddr;
		e_e = e820[i].baseaddr + e820[i].length;
		if ((e_s <= start) && ((start + size) <= e_e)) {
			int index_s = 0, index_e = 3;
			uint64_t pt[4];
			uint32_t pt_t[3];

			pt[0] = e_s;
			pt[1] = start;
			pt[2] = start + size;
			pt[3] = e_e;

			pt_t[0] = e820[i].type;
			pt_t[1] = type;
			pt_t[2] = e820[i].type;

			if (e_s == start) {
				index_s = 1;
			}

			if (e_e == (start + size)) {
				index_e = 2;
			}
			length += index_e - index_s - 1;

			if ((i != (len - 1) && ((index_e - index_s) > 1))) {
				memmove(&e820[i + index_e - index_s],
					&e820[i + 1], (len - i - 1) *
					sizeof(struct e820_entry));
			}

			for (; index_s < index_e; index_s++, i++) {
				e820[i].baseaddr = pt[index_s];
				e820[i].length = pt[index_s + 1] - pt[index_s];
				e820[i].type = pt_t[index_s];
			}

			break;
		}
	}

	return length;
}

uint32_t
acrn_create_e820_table(struct vmctx *ctx, struct e820_entry *e820)
{
	uint32_t removed = 0, k;

	memcpy(e820, e820_default_entries, sizeof(e820_default_entries));
	if (ctx->lowmem <= e820[LOWRAM_E820_ENTRY + 2].baseaddr) {
		e820[LOWRAM_E820_ENTRY].length =
			(ctx->lowmem < e820[LOWRAM_E820_ENTRY+1].baseaddr ? ctx->lowmem :
			e820[LOWRAM_E820_ENTRY+1].baseaddr) - e820[LOWRAM_E820_ENTRY].baseaddr;

		memmove(&e820[LOWRAM_E820_ENTRY + 2], &e820[LOWRAM_E820_ENTRY + 3],
				sizeof(struct e820_entry) *
				(NUM_E820_ENTRIES - (LOWRAM_E820_ENTRY + 3)));
		removed++;
	} else {
		e820[LOWRAM_E820_ENTRY].length = e820[LOWRAM_E820_ENTRY+1].baseaddr -
			e820[LOWRAM_E820_ENTRY].baseaddr;

		e820[LOWRAM_E820_ENTRY + 2].length =
			((ctx->lowmem < e820[LOWRAM_E820_ENTRY + 3].baseaddr) ? ctx->lowmem :
			e820[LOWRAM_E820_ENTRY + 3].baseaddr) - e820[LOWRAM_E820_ENTRY + 2].baseaddr;
	}

	/* remove [5GB, highmem) if it's empty */
	if (ctx->highmem > 0) {
		e820[HIGHRAM_E820_ENTRY - removed].type = E820_TYPE_RAM;
		e820[HIGHRAM_E820_ENTRY - removed].length = ctx->highmem;
	} else {
		removed++;
	}

	pr_info("SW_LOAD: build e820 %d entries to addr: %p\r\n",
			NUM_E820_ENTRIES - removed, (void *)e820);

	for (k = 0; k < NUM_E820_ENTRIES - removed; k++)
		pr_info("SW_LOAD: entry[%d]: addr 0x%016lx, size 0x%016lx, "
				" type 0x%x\r\n",
				k, e820[k].baseaddr,
				e820[k].length,
				e820[k].type);

	return (NUM_E820_ENTRIES - removed);
}

int
acrn_sw_load(struct vmctx *ctx)
{
	if (vsbl_file_name)
		return acrn_sw_load_vsbl(ctx);
	else if ((ovmf_file_name != NULL) ^ (ovmf_code_file_name && ovmf_vars_file_name))
		return acrn_sw_load_ovmf(ctx);
	else if (kernel_file_name)
		return acrn_sw_load_bzimage(ctx);
	else if (elf_file_name)
		return acrn_sw_load_elf(ctx);
	else
		return -1;
}
