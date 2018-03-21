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
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

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
 * 0:             0 -     lowmem      RAM          lowmem
 * 1:        lowmem -     bff_fffff   (reserved)   0xc00_00000-lowmem
 * 2:   0xc00_00000 -     dff_fffff   PCI hole     512MB
 * 3:   0xe00_00000 -     fff_fffff   (reserved)   512MB
 * 2:   1_000_00000 -     highmem     RAM          highmem-4G
 */
const struct e820_entry e820_default_entries[NUM_E820_ENTRIES] = {
	{	/* 0 to lowmem */
		.baseaddr =  0x00000000,
		.length   =  0x49000000,
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
	int len = strlen(arg);

	if (len < STR_LEN) {
		strncpy(bootargs, arg, len);
		bootargs[len] = '\0';
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
check_image(char *path)
{
	FILE *fp;

	fp = fopen(path, "r");
	if (fp == NULL)
		return -1;

	fclose(fp);
	return 0;
}

uint32_t
acrn_create_e820_table(struct vmctx *ctx, struct e820_entry *e820)
{
	uint32_t k;

	memcpy(e820, e820_default_entries, sizeof(e820_default_entries));

	if (ctx->lowmem > 0) {
		e820[LOWRAM_E820_ENTRIES].length = ctx->lowmem;
		e820[LOWRAM_E820_ENTRIES+1].baseaddr = ctx->lowmem;
		e820[LOWRAM_E820_ENTRIES+1].length =
			ctx->lowmem_limit - ctx->lowmem;
	}

	if (ctx->highmem > 0) {
		e820[HIGHRAM_E820_ENTRIES].type = E820_TYPE_RAM;
		e820[HIGHRAM_E820_ENTRIES].length = ctx->highmem;
	}

	printf("SW_LOAD: build e820 %d entries to addr: %p\n",
			NUM_E820_ENTRIES, (void *)e820);

	for (k = 0; k < NUM_E820_ENTRIES; k++)
		printf("SW_LOAD: entry[%d]: addr 0x%016lx, size 0x%016lx, "
				" type 0x%x\n",
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
	else
		return acrn_sw_load_bzimage(ctx);
}
