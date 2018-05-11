/*-
 * Copyright (c) 2018 Intel Corporation
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
#include "sw_load.h"

#define SETUP_SIG 0x5a5aaa55

/* If we load kernel/ramdisk/bootargs directly, the UOS
 * memory layout will be like:
 *
 * | ...                                                 |
 * +-----------------------------------------------------+
 * | offset: 0xf2400 (ACPI table)                        |
 * +-----------------------------------------------------+
 * | ...                                                 |
 * +-----------------------------------------------------+
 * | offset: 16MB (kernel image)                         |
 * +-----------------------------------------------------+
 * | ...                                                 |
 * +-----------------------------------------------------+
 * | offset: lowmem - 4MB (ramdisk image)                |
 * +-----------------------------------------------------+
 * | offset: lowmem - 8K (bootargs)                      |
 * +-----------------------------------------------------+
 * | offset: lowmem - 6K (kernel entry address)          |
 * +-----------------------------------------------------+
 * | offset: lowmem - 4K (zero_page include e820 table)  |
 * +-----------------------------------------------------+
 */

/* Check default e820 table in sw_load_common.c for info about ctx->lowmem */
#define RAMDISK_LOAD_OFF(ctx)	(ctx->lowmem - 4*MB)
#define BOOTARGS_LOAD_OFF(ctx)	(ctx->lowmem - 8*KB)
#define KERNEL_ENTRY_OFF(ctx)	(ctx->lowmem - 6*KB)
#define ZEROPAGE_LOAD_OFF(ctx)	(ctx->lowmem - 4*KB)
#define KERNEL_LOAD_OFF(ctx)	(16*MB)

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

static char ramdisk_path[STR_LEN];
static char kernel_path[STR_LEN];
static int with_ramdisk;
static int with_kernel;
static int ramdisk_size;
static int kernel_size;

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

int
acrn_parse_kernel(char *arg)
{
	int len = strlen(arg);

	if (len < STR_LEN) {
		strncpy(kernel_path, arg, len);
		kernel_path[len] = '\0';
		if (check_image(kernel_path) != 0){
			fprintf(stderr, "SW_LOAD: check_image failed for '%s'\n",
				kernel_path);
			exit(10); /* Non-zero */
		}
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
		if (check_image(ramdisk_path) != 0){
			fprintf(stderr, "SW_LOAD: check_image failed for '%s'\n",
				ramdisk_path);
			exit(11); /* Non-zero */
		}

		with_ramdisk = 1;
		printf("SW_LOAD: get ramdisk path %s\n", ramdisk_path);
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
acrn_sw_load_bzimage(struct vmctx *ctx)
{
	int ret, setup_size;
	uint64_t *cfg_offset = (uint64_t *)(ctx->baseaddr + GUEST_CFG_OFFSET);

	*cfg_offset = ctx->lowmem;

	if (with_bootargs) {
		strcpy(ctx->baseaddr + BOOTARGS_LOAD_OFF(ctx), get_bootargs());
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

