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

#include "acrn_common.h"
#include "vmmapi.h"

#define STR_LEN	1024

#define SETUP_SIG 0x5a5aaa55

#define	KB	(1024UL)
#define	MB	(1024 * 1024UL)
#define	GB	(1024 * 1024 * 1024UL)

/* E820 memory types */
#define E820_TYPE_RAM           1   /* EFI 1, 2, 3, 4, 5, 6, 7 */
/* EFI 0, 11, 12, 13 (everything not used elsewhere) */
#define E820_TYPE_RESERVED      2
#define E820_TYPE_ACPI_RECLAIM  3   /* EFI 9 */
#define E820_TYPE_ACPI_NVS      4   /* EFI 10 */
#define E820_TYPE_UNUSABLE      5   /* EFI 8 */

#define NUM_E820_ENTRIES	4
#define LOWRAM_E820_ENTRIES	0
#define HIGHRAM_E820_ENTRIES	3

/* see below e820 default mapping for more info about ctx->lowmem */
#define RAMDISK_LOAD_OFF(ctx)	(ctx->lowmem - 4*MB)
#define BOOTARGS_LOAD_OFF(ctx)	(ctx->lowmem - 8*KB)
#define KERNEL_ENTRY_OFF(ctx)	(ctx->lowmem - 6*KB)
#define ZEROPAGE_LOAD_OFF(ctx)	(ctx->lowmem - 4*KB)
#define KERNEL_LOAD_OFF(ctx)	(16*MB)

/* Defines a single entry in an E820 memory map. */
struct e820_entry {
	/** The base address of the memory range. */
	uint64_t baseaddr;
	/** The length of the memory range. */
	uint64_t length;
	/** The type of memory region. */
	uint32_t type;
} __attribute__((packed));

/* The real mode kernel header, refer to Documentation/x86/boot.txt */
struct _zeropage {
	uint8_t pad1[0x1e8];                    /* 0x000 */
	uint8_t e820_nentries;                  /* 0x1e8 */
	uint8_t pad2[0x8];                      /* 0x1e9 */

	struct	{
		uint8_t hdr_pad1[0x1f];         /* 0x1f1 */
		uint8_t loader_type;            /* 0x210 */
		uint8_t load_flags;             /* 0x211 */
		uint8_t hdr_pad2[0x2];          /* 0x212 */
		uint32_t code32_start;          /* 0x214 */
		uint32_t ramdisk_addr;          /* 0x218 */
		uint32_t ramdisk_size;          /* 0x21c */
		uint8_t hdr_pad3[0x8];          /* 0x220 */
		uint32_t bootargs_addr;         /* 0x228 */
		uint8_t hdr_pad4[0x3c];         /* 0x22c */
	} __attribute__((packed)) hdr;

	uint8_t pad3[0x68];                     /* 0x268 */
	struct e820_entry e820[0x80];           /* 0x2d0 */
	uint8_t pad4[0x330];                    /* 0xcd0 */
} __attribute__((packed));

static char bootargs[STR_LEN];
static char ramdisk_path[STR_LEN];
static char kernel_path[STR_LEN];
static int with_bootargs;
static int with_ramdisk;
static int with_kernel;
static int ramdisk_size;
static int kernel_size;

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

static int
acrn_get_bzimage_setup_size(struct vmctx *ctx)
{
	uint32_t *tmp, location = 1024, setup_sectors;
	int size = -1;

	tmp = (uint32_t *)(ctx->baseaddr + KERNEL_LOAD_OFF(ctx)) + 1024/4;
	while (*tmp != SETUP_SIG && location < 0x8000) {
		tmp++;
		location += 4;
	}

	/* setup size must be at least 1024 bytes and small than 0x8000 */
	if (location < 0x8000 && location > 1024) {
		setup_sectors = (location + 511) / 512;
		size = setup_sectors*512;
		printf("SW_LOAD: found setup sig @ 0x%08x, "
				"setup_size is 0x%08x\n",
				location, size);
	} else
		printf("SW_LOAD ERR: could not get setup "
				"size in kernel %s\n",
				kernel_path);
	return size;
}

static int
check_image(char *path)
{
	FILE *fp;

	fp = fopen(path, "r");
	if (fp == NULL)
		return -1;

	fclose(fp);
	return 0;
}

int
acrn_parse_kernel(char *arg)
{
	int len = strlen(arg);

	if (len < STR_LEN) {
		strncpy(kernel_path, arg, len);
		kernel_path[len] = '\0';
		assert(check_image(kernel_path) == 0);

		with_kernel = 1;
		printf("SW_LOAD: get kernel path %s\n", kernel_path);
		return 0;
	} else
		return -1;
}

int
acrn_parse_ramdisk(char *arg)
{
	int len = strlen(arg);

	if (len < STR_LEN) {
		strncpy(ramdisk_path, arg, len);
		ramdisk_path[len] = '\0';
		assert(check_image(ramdisk_path) == 0);

		with_ramdisk = 1;
		printf("SW_LOAD: get ramdisk path %s\n", ramdisk_path);
		return 0;
	} else
		return -1;
}

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

static int
acrn_prepare_ramdisk(struct vmctx *ctx)
{
	FILE *fp;
	int len, read;

	fp = fopen(ramdisk_path, "r");
	if (fp == NULL) {
		printf("SW_LOAD ERR: could not open ramdisk file %s\n",
				ramdisk_path);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	if (len > (BOOTARGS_LOAD_OFF(ctx) - RAMDISK_LOAD_OFF(ctx))) {
		printf("SW_LOAD ERR: the size of ramdisk file is too big"
				" file len=0x%x, limit is 0x%lx\n", len,
				BOOTARGS_LOAD_OFF(ctx) - RAMDISK_LOAD_OFF(ctx));
		fclose(fp);
		return -1;
	}
	ramdisk_size = len;

	fseek(fp, 0, SEEK_SET);
	read = fread(ctx->baseaddr + RAMDISK_LOAD_OFF(ctx),
			sizeof(char), len, fp);
	if (read < len) {
		printf("SW_LOAD ERR: could not read the whole ramdisk file,"
				" file len=%d, read %d\n", len, read);
		fclose(fp);
		return -1;
	}
	fclose(fp);
	printf("SW_LOAD: ramdisk %s size %d copied to guest 0x%lx\n",
			ramdisk_path, ramdisk_size, RAMDISK_LOAD_OFF(ctx));

	return 0;
}

static int
acrn_prepare_kernel(struct vmctx *ctx)
{
	FILE *fp;
	int len, read;

	fp = fopen(kernel_path, "r");
	if (fp == NULL) {
		printf("SW_LOAD ERR: could not open kernel file %s\n",
				kernel_path);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	if ((len + KERNEL_LOAD_OFF(ctx)) > RAMDISK_LOAD_OFF(ctx)) {
		printf("SW_LOAD ERR: need big system memory to fit image\n");
		fclose(fp);
		return -1;
	}
	kernel_size = len;

	fseek(fp, 0, SEEK_SET);
	read = fread(ctx->baseaddr + KERNEL_LOAD_OFF(ctx),
			sizeof(char), len, fp);
	if (read < len) {
		printf("SW_LOAD ERR: could not read the whole kernel file,"
				" file len=%d, read %d\n", len, read);
		fclose(fp);
		return -1;
	}
	fclose(fp);
	printf("SW_LOAD: kernel %s size %d copied to guest 0x%lx\n",
			kernel_path, kernel_size, KERNEL_LOAD_OFF(ctx));

	return 0;
}

static uint32_t
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

	return NUM_E820_ENTRIES;
}

static int
acrn_prepare_zeropage(struct vmctx *ctx, int setup_size)
{
	struct _zeropage *zeropage = (struct _zeropage *)
		(ctx->baseaddr + ZEROPAGE_LOAD_OFF(ctx));
	struct _zeropage *kernel_load = (struct _zeropage *)
		(ctx->baseaddr + KERNEL_LOAD_OFF(ctx));

	/* clear the zeropage */
	memset(zeropage, 0, 2*KB);

	/* copy part of the header into the zero page */
	memcpy(&(zeropage->hdr), &(kernel_load->hdr), sizeof(zeropage->hdr));

	if (with_ramdisk) {
		/*Copy ramdisk load_addr and size in zeropage header structure*/
		zeropage->hdr.ramdisk_addr = (uint32_t)
			((uint64_t)RAMDISK_LOAD_OFF(ctx));
		zeropage->hdr.ramdisk_size = (uint32_t)ramdisk_size;

		printf("SW_LOAD: build zeropage for ramdisk addr: 0x%x,"
				" size: %d\n", zeropage->hdr.ramdisk_addr,
				zeropage->hdr.ramdisk_size);
	}

	/* Copy bootargs load_addr in zeropage header structure */
	zeropage->hdr.bootargs_addr = (uint32_t)
		((uint64_t)BOOTARGS_LOAD_OFF(ctx));
	printf("SW_LOAD: build zeropage for bootargs addr: 0x%x\n",
			zeropage->hdr.bootargs_addr);

	/* set constant arguments in zero page */
	zeropage->hdr.loader_type     = 0xff;
	zeropage->hdr.load_flags     |= (1<<5); /* quiet */

	/* Create/add e820 table entries in zeropage */
	zeropage->e820_nentries = acrn_create_e820_table(ctx, zeropage->e820);

	return 0;
}

int
acrn_sw_load(struct vmctx *ctx)
{
	int ret, setup_size;
	uint64_t *cfg_offset = (uint64_t *)(ctx->baseaddr + GUEST_CFG_OFFSET);

	*cfg_offset = ctx->lowmem;

	if (with_bootargs) {
		strcpy(ctx->baseaddr + BOOTARGS_LOAD_OFF(ctx), bootargs);
		printf("SW_LOAD: bootargs copied to guest 0x%lx\n",
				BOOTARGS_LOAD_OFF(ctx));
	}

	if (with_ramdisk) {
		ret = acrn_prepare_ramdisk(ctx);
		if (ret)
			return ret;
	}

	if (with_kernel) {
		uint64_t *kernel_entry_addr =
			(uint64_t *)(ctx->baseaddr + KERNEL_ENTRY_OFF(ctx));

		ret = acrn_prepare_kernel(ctx);
		if (ret)
			return ret;
		setup_size = acrn_get_bzimage_setup_size(ctx);
		if (setup_size <= 0)
			return -1;
		*kernel_entry_addr = (uint64_t)
			(KERNEL_LOAD_OFF(ctx) + setup_size + 0x200);
		ret = acrn_prepare_zeropage(ctx, setup_size);
		if (ret)
			return ret;

		printf("SW_LOAD: zeropage prepared @ 0x%lx, "
				"kernel_entry_addr=0x%lx\n",
				ZEROPAGE_LOAD_OFF(ctx), *kernel_entry_addr);
	}

	return 0;
}
