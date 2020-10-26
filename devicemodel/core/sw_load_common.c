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
#include "ptct.h"

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
 *             Begin    Limit       Type            Length
 * 0:              0 -  0xA0000     RAM             0xA0000
 * 1:        0xA0000 -  0x100000    (reserved)      0x60000
 * 2:       0x100000 -  lowmem      RAM             lowmem - 1MB
 * 3:         lowmem -  0x80000000  (reserved)      2GB - lowmem
 * 4:	  0x80000000 -  0x88000000  (reserved)	    128MB
 * 5:     0xDB000000 -  0xDF000000  (reserved)      64MB
 * 6:     0xDF000000 -  0xE0000000  (reserved)      16MB
 * 7:     0xE0000000 -  0x100000000 MCFG, MMIO      512MB
 * 8:    0x100000000 -  0x140000000 64-bit PCI hole 1GB
 * 9:    0x140000000 -  highmem     RAM             highmem - 5GB
 */
const struct e820_entry e820_default_entries[NUM_E820_ENTRIES] = {
	{	/* 0 to video memory */
		.baseaddr = 0x00000000,
		.length   = 0xA0000,
		.type     = E820_TYPE_RAM
	},

	{	/* video memory, ROM (used for e820/mptable/smbios/acpi) */
		.baseaddr = 0xA0000,
		.length   = 0x60000,
		.type     = E820_TYPE_RESERVED
	},

	{	/* 1MB to lowmem */
		.baseaddr = 0x100000,
		.length   = PSRAM_BASE_GPA - 0x100000,
		.type     = E820_TYPE_RAM
	},

	{	/* pSRAM area */
		.baseaddr = PSRAM_BASE_GPA,
		.length   = PSRAM_MAX_SIZE,
		.type     = E820_TYPE_RESERVED
	},

	{	/*
			This is the dynamic extra segment. When pSRAM exists, this part may be as either the head part of reserved (ummapped) lowmem or the tail part of usable lowmem.
			When lowmem > PSRAM_BASE_GPA + PSRAM_MAX_SIZE:
				This region is the tail part of usable lowmem, and the length is lowmem -  (PSRAM_BASE_GPA + PSRAM_MAX_SIZE)
			When lowmem < PSRAM_BASE_GPA:
				This region is the head part of reserved (ummapped) lowmem, and the length is PSRAM_BASE_GPA - lowmem.
				Also, it should swap the position with pSRAM part, and set to be reserved.
			Otherwise:
				This region will be removed since its length is 0.
		 */
		.baseaddr = PSRAM_BASE_GPA + PSRAM_MAX_SIZE,
		.length   = 0,
		.type     = E820_TYPE_RAM
	},

	{	/* lowmem to lowmem_limit */
		.baseaddr = PSRAM_BASE_GPA + PSRAM_MAX_SIZE,
		.length   = 2*GB - (PSRAM_BASE_GPA + PSRAM_MAX_SIZE),
		.type     = E820_TYPE_RESERVED
	},

	{
		/* reserve for PRM resource */
		.baseaddr = 0x80000000,
		.length	  = 0x8000000,
		.type     = E820_TYPE_RESERVED
	},

	{
		/* reserve for GVT-d graphics stolen memory.
		 * The native BIOS allocates the stolen memory by itself,
		 * and size can be configured by user itself through BIOS GUI.
		 * For ACRN, we simply hard code to 64MB and static
		 * reserved the memory region to avoid more efforts in OVMF,
		 * and user *must* align the native BIOS setting to 64MB.
		 *
		 * GPU_GSM_GPA micro in passthrough.c should
+		 * align with this address here.
		 */
		.baseaddr = 0xDB000000,
		.length	  = 0x4000000,
		.type     = E820_TYPE_RESERVED
	},

	{
		/* reserve for GVT */
		.baseaddr = 0xDF000000,
		.length	  = 0x1000000,
		.type     = E820_TYPE_RESERVED
	},

	{	/* ECFG_BASE to 4GB */
		.baseaddr = PCI_EMUL_ECFG_BASE,
		.length   = (4 * GB) - PCI_EMUL_ECFG_BASE,
		.type     = E820_TYPE_RESERVED
	},

	{	/* 4GB to 5GB */
		.baseaddr = PCI_EMUL_MEMBASE64,
		.length   = PCI_EMUL_MEMLIMIT64 - PCI_EMUL_MEMBASE64,
		.type     = E820_TYPE_RESERVED
	},

	{	/* 5GB to highmem */
		.baseaddr = PCI_EMUL_MEMLIMIT64,
		.length   = 0x000100000,
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
	uint32_t e820_entries_num = NUM_E820_ENTRIES, k;
	uint32_t lowmem_ram_seg_idx = LOWRAM_E820_ENTRY;
	uint32_t psram_seg_idx = LOWRAM_E820_ENTRY+1;
	uint32_t dynamic_seg_idx = LOWRAM_E820_ENTRY+2;
	uint32_t lowmem_reserved_seg_idx = LOWRAM_E820_ENTRY+3;
	uint32_t highmem_seg_idx = HIGHRAM_E820_ENTRY;
	struct e820_entry tmp_entry;

	memcpy(e820, e820_default_entries, sizeof(e820_default_entries));
	if (!pt_ptct) {
		/* When there is no pSRAM for this VM, we should firstly remove the pSRAM segment and dynamic extra part from guest E820 */
		memmove(&e820[psram_seg_idx], &e820[lowmem_reserved_seg_idx],
					sizeof(struct e820_entry) *
					(e820_entries_num - lowmem_reserved_seg_idx));
		e820_entries_num-=2;
		lowmem_reserved_seg_idx-=2;

		e820[lowmem_ram_seg_idx].length = ctx->lowmem -
				e820[lowmem_ram_seg_idx].baseaddr;

		/* remove [lowmem, lowmem_limit) if it's empty */
		if (ctx->lowmem_limit > ctx->lowmem) {
			e820[lowmem_reserved_seg_idx].baseaddr = ctx->lowmem;
			e820[lowmem_reserved_seg_idx].length =
				ctx->lowmem_limit - ctx->lowmem;
		} else {
			memmove(&e820[lowmem_reserved_seg_idx], &e820[lowmem_reserved_seg_idx+1],
					sizeof(struct e820_entry) *
					(e820_entries_num - (lowmem_reserved_seg_idx+1)));
			e820_entries_num--;
		}
	} else {
		/* Fix-Me: Currently, when memory size < 1GB and --psram is added to DM cmdline, the boot may fail. Needs further debug & refine*/
		if (ctx->lowmem < PSRAM_BASE_GPA) {
		/* In this case, dynamic extra segment is the head part of reserved lowmem. We should swap its position with pSRAM area */
			tmp_entry = e820[psram_seg_idx];
			psram_seg_idx = lowmem_ram_seg_idx + 2;
			dynamic_seg_idx = lowmem_ram_seg_idx + 1;
			e820[psram_seg_idx] = tmp_entry;
			e820[dynamic_seg_idx].baseaddr = ctx->lowmem;
			e820[dynamic_seg_idx].length = PSRAM_BASE_GPA - ctx->lowmem;
			e820[dynamic_seg_idx].type = E820_TYPE_RESERVED;
		} else if (ctx->lowmem > PSRAM_BASE_GPA + PSRAM_MAX_SIZE) {
		/* In this case, dynamic extra segment is the tail part of usable lowmem. */
			if (ctx->lowmem_limit > ctx->lowmem) {
				e820[dynamic_seg_idx].length = ctx->lowmem - (PSRAM_BASE_GPA + PSRAM_MAX_SIZE);
				e820[lowmem_reserved_seg_idx].baseaddr = ctx->lowmem;
				e820[lowmem_reserved_seg_idx].length = ctx->lowmem_limit - ctx->lowmem;
			} else {
				e820[dynamic_seg_idx].length = ctx->lowmem_limit - (PSRAM_BASE_GPA + PSRAM_MAX_SIZE);
				/* When lowmem > lowmem_limit, all lowmem is usable, so we will remove the reserved lowmem segment */
				memmove(&e820[lowmem_reserved_seg_idx], &e820[lowmem_reserved_seg_idx+1],
					sizeof(struct e820_entry) *
					(e820_entries_num - (lowmem_reserved_seg_idx+1)));
				e820_entries_num--;
			}
		} else {
			/* When PSRAM_BASE_GPA <= lowmem <= PSRAM_BASE_GPA + PSRAM_MAX_SIZE, we just remove the dynamic extra segment */
			memmove(&e820[dynamic_seg_idx], &e820[dynamic_seg_idx+1],
					sizeof(struct e820_entry) *
					(e820_entries_num - (dynamic_seg_idx+1)));
			e820_entries_num--;
		}
	}

	/* remove [5GB, highmem) if it's empty */
	highmem_seg_idx -= (NUM_E820_ENTRIES - e820_entries_num);
	if (ctx->highmem > 0) {
		e820[highmem_seg_idx].type = E820_TYPE_RAM;
		e820[highmem_seg_idx].length = ctx->highmem;
	} else {
		e820_entries_num--;
	}

	pr_info("SW_LOAD: build e820 %d entries to addr: %p\r\n",
			e820_entries_num, (void *)e820);

	for (k = 0; k < e820_entries_num; k++)
		pr_info("SW_LOAD: entry[%d]: addr 0x%016lx, size 0x%016lx, "
				" type 0x%x\r\n",
				k, e820[k].baseaddr,
				e820[k].length,
				e820[k].type);

	return e820_entries_num;
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
