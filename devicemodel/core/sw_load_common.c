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

int with_bootargs;
static char bootargs[STR_LEN];

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
 *             Begin      End         Type         Length
 * 0:             0 -     0xef000     RAM          0xEF000
 * 1	    0xef000 -	  0x100000    (reserved)   0x11000
 * 2       0x100000 -	  lowmem      RAM          lowmem - 0x100000
 * 3:        lowmem -     bff_fffff   (reserved)   0xc00_00000-lowmem
 * 4:   0xc00_00000 -     dff_fffff   PCI hole     512MB
 * 5:   0xe00_00000 -     fff_fffff   (reserved)   512MB
 * 6:   1_000_00000 -     highmem     RAM          highmem-4G
 */
const struct e820_entry e820_default_entries[NUM_E820_ENTRIES] = {
	{	/* 0 to mptable/smbios/acpi */
		.baseaddr =  0x00000000,
		.length   =  0xEF000,
		.type     =  E820_TYPE_RAM
	},

	{	/* guest_cfg_addr/mptable/smbios/acpi to lowmem */
		.baseaddr = 0xEF000,
		.length	  = 0x11000,
		.type	  = E820_TYPE_RESERVED
	},

	{	/* lowmem to lowmem_limit*/
		.baseaddr =  0x100000,
		.length   =  0x48f00000,
		.type     =  E820_TYPE_RAM
	},

	{	/* lowmem to lowmem_limit*/
		.baseaddr =  0x49000000,
		.length   =  0x77000000,
		.type     =  E820_TYPE_RESERVED
	},

	{	/* lowmem_limit to 4G */
		.baseaddr =  0xe0000000,
		.length   =  0x20000000,
		.type     =  E820_TYPE_RESERVED
	},

	{
		.baseaddr =  0x100000000,
		.length   =  0x000100000,
		.type     =  E820_TYPE_RESERVED
	},
};

int
acrn_parse_bootargs(char *arg)
{
	size_t len = strnlen(arg, STR_LEN);

	if (len < STR_LEN) {
		strncpy(bootargs, arg, len + 1);
		with_bootargs = 1;
		printf("SW_LOAD: get bootargs %s\n", bootargs);
		return 0;
	} else
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
		fprintf(stderr,
			"SW_LOAD ERR: image file failed to open\n");
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);

	if (len == 0 || (size_limit && len > size_limit)) {
		fprintf(stderr,
			"SW_LOAD ERR: file is %s\n",
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
	uint32_t k;

	memcpy(e820, e820_default_entries, sizeof(e820_default_entries));

	if (ctx->lowmem > 0) {
		e820[LOWRAM_E820_ENTRIES].length = ctx->lowmem -
				e820[LOWRAM_E820_ENTRIES].baseaddr;
		e820[LOWRAM_E820_ENTRIES+1].baseaddr = ctx->lowmem;
		e820[LOWRAM_E820_ENTRIES+1].length =
			ctx->lowmem_limit - ctx->lowmem;
	}

	if (ctx->highmem > 0) {
		e820[HIGHRAM_E820_ENTRIES].type = E820_TYPE_RAM;
		e820[HIGHRAM_E820_ENTRIES].length = ctx->highmem;
	}

	printf("SW_LOAD: build e820 %d entries to addr: %p\r\n",
			NUM_E820_ENTRIES, (void *)e820);

	for (k = 0; k < NUM_E820_ENTRIES; k++)
		printf("SW_LOAD: entry[%d]: addr 0x%016lx, size 0x%016lx, "
				" type 0x%x\r\n",
				k, e820[k].baseaddr,
				e820[k].length,
				e820[k].type);

	return  NUM_E820_ENTRIES;
}

int
acrn_sw_load(struct vmctx *ctx)
{
	if (vsbl_file_name)
		return acrn_sw_load_vsbl(ctx);
	else if (ovmf_file_name)
		return acrn_sw_load_ovmf(ctx);
	else if (kernel_file_name)
		return acrn_sw_load_bzimage(ctx);
	else if (elf_file_name)
		return acrn_sw_load_elf(ctx);
	else
		return -1;
}
