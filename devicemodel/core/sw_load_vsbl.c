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

#include "dm.h"
#include "acrn_common.h"
#include "vmmapi.h"
#include "sw_load.h"
#include "acpi.h"


/* If the vsbl is loaded by DM, the UOS memory layout will be like:
 *
 * | ...                                              |
 * +--------------------------------------------------+
 * | offset: 0xf2400 (ACPI table)                     |
 * +--------------------------------------------------+
 * | ...                                              |
 * +--------------------------------------------------+
 * | offset: 16MB (vsbl image)                        |
 * +--------------------------------------------------+
 * | ...                                              |
 * +--------------------------------------------------+
 * | offset: lowmem - 16K (partition blob)            |
 * +--------------------------------------------------+
 * | offset: lowmem - 12K (e820 table)                |
 * +--------------------------------------------------+
 * | offset: lowmem - 8K (boot_args_address)          |
 * +--------------------------------------------------+
 * | offset: lowmem - 6K (vsbl entry address)         |
 * +--------------------------------------------------+
 * | offset: lowmem - 4K (config_page with e820 table)|
 * +--------------------------------------------------+
 */

/* Check default e820 table in sw_load_common.c for info about ctx->lowmem */
#define	CONFIGPAGE_OFF(ctx)	((ctx)->lowmem - 4*KB)
#define	VSBL_ENTRY_OFF(ctx)	((ctx)->lowmem - 6*KB)
#define	BOOTARGS_OFF(ctx)	((ctx)->lowmem - 8*KB)
#define	E820_TABLE_OFF(ctx)	((ctx)->lowmem - 12*KB)
#define	GUEST_PART_INFO_OFF(ctx)	((ctx)->lowmem - 16*KB)
/* vsbl real entry is saved in the first 4 bytes of vsbl image */
#define	VSBL_OFF(ctx)		(16*MB)

struct vsbl_para {
	uint64_t		e820_table_address;
	uint64_t		e820_entries;
	uint64_t		acpi_table_address;
	uint64_t		acpi_table_size;
	uint64_t		guest_part_info_address;
	uint64_t		guest_part_info_size;
	uint64_t		vsbl_address;
	uint64_t		vsbl_size;
	uint64_t		bootargs_address;
	uint32_t		trusty_enabled;
	uint32_t		key_info_lock;
	uint32_t		watchdog_reset;
	uint32_t		boot_device_address;
};

static char guest_part_info_path[STR_LEN];
static int guest_part_info_size;
static bool with_guest_part_info;

static char vsbl_path[STR_LEN];
static int vsbl_size;

static int boot_blk_bdf;

#define	LOW_8BIT(x)	((x) & 0xFF)
void
vsbl_set_bdf(int bnum, int snum, int fnum)
{
	boot_blk_bdf = (LOW_8BIT(bnum) << 16) | (LOW_8BIT(snum) << 8) |
		LOW_8BIT(fnum);
}

int
acrn_parse_guest_part_info(char *arg)
{
	size_t len = strlen(arg);

	if (len < STR_LEN) {
		strncpy(guest_part_info_path, arg, len + 1);
		assert(check_image(guest_part_info_path) == 0);

		with_guest_part_info = true;

		printf("SW_LOAD: get partition blob path %s\n",
			guest_part_info_path);
		return 0;
	} else
		return -1;
}

static int
acrn_prepare_guest_part_info(struct vmctx *ctx)
{
	FILE *fp;
	int len, read;

	fp = fopen(guest_part_info_path, "r");
	if (fp == NULL) {
		fprintf(stderr,
			"SW_LOAD ERR: could not open partition blob %s\n",
			guest_part_info_path);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	if ((len + GUEST_PART_INFO_OFF(ctx)) > BOOTARGS_OFF(ctx)) {
		fprintf(stderr,
			"SW_LOAD ERR: too large partition blob\n");
		fclose(fp);
		return -1;
	}

	guest_part_info_size = len;

	fseek(fp, 0, SEEK_SET);
	read = fread(ctx->baseaddr + GUEST_PART_INFO_OFF(ctx), 
		sizeof(char), len, fp);
	if (read < len) {
		fprintf(stderr,
			"SW_LOAD ERR: could not read whole partition blob\n");
		fclose(fp);
		return -1;
	}
	fclose(fp);
	printf("SW_LOAD: partition blob %s size %d copy to guest 0x%lx\n",
		guest_part_info_path, guest_part_info_size,
		GUEST_PART_INFO_OFF(ctx));

	return 0;
}

int
acrn_parse_vsbl(char *arg)
{
	size_t len = strlen(arg);

	if (len < STR_LEN) {
		strncpy(vsbl_path, arg, len + 1);
		assert(check_image(vsbl_path) == 0);

		vsbl_file_name = vsbl_path;

		printf("SW_LOAD: get vsbl path %s\n",
			vsbl_path);
		return 0;
	} else
		return -1;
}

static int
acrn_prepare_vsbl(struct vmctx *ctx)
{
	FILE *fp;
	int len, read;

	fp = fopen(vsbl_path, "r");
	if (fp == NULL) {
		fprintf(stderr,
			"SW_LOAD ERR: could not open vsbl file: %s\n",
			vsbl_path);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	if ((len + VSBL_OFF(ctx)) > GUEST_PART_INFO_OFF(ctx)) {
		fprintf(stderr,
			"SW_LOAD ERR: too large vsbl file\n");
		fclose(fp);
		return -1;
	}

	vsbl_size = len;

	fseek(fp, 0, SEEK_SET);
	read = fread(ctx->baseaddr + VSBL_OFF(ctx),
		sizeof(char), len, fp);
	if (read < len) {
		fprintf(stderr,
			"SW_LOAD ERR: could not read whole partition blob\n");
		fclose(fp);
		return -1;
	}
	fclose(fp);
	printf("SW_LOAD: partition blob %s size %d copy to guest 0x%lx\n",
		vsbl_path, vsbl_size, VSBL_OFF(ctx));

	return 0;
}

int
acrn_sw_load_vsbl(struct vmctx *ctx)
{
	int ret;
	struct e820_entry *e820;
	struct vsbl_para *vsbl_para;
	uint64_t vsbl_start_addr =
		(uint64_t)ctx->baseaddr + VSBL_OFF(ctx);
	uint64_t *vsbl_entry =
		(uint64_t *)(ctx->baseaddr + VSBL_ENTRY_OFF(ctx));
	uint64_t *cfg_offset =
		(uint64_t *)(ctx->baseaddr + GUEST_CFG_OFFSET);

	*cfg_offset = ctx->lowmem;

	vsbl_para = (struct vsbl_para *)
		(ctx->baseaddr + CONFIGPAGE_OFF(ctx));

	memset(vsbl_para, 0x0, sizeof(struct vsbl_para));

	e820 = (struct e820_entry *)
		(ctx->baseaddr + E820_TABLE_OFF(ctx));

	vsbl_para->e820_entries = acrn_create_e820_table(ctx, e820);
	vsbl_para->e820_table_address = E820_TABLE_OFF(ctx);

	vsbl_para->acpi_table_address = get_acpi_base();
	vsbl_para->acpi_table_size = get_acpi_table_length();

	if (with_bootargs) {
		strcpy(ctx->baseaddr + BOOTARGS_OFF(ctx), get_bootargs());
		vsbl_para->bootargs_address = BOOTARGS_OFF(ctx);
	} else {
		vsbl_para->bootargs_address = 0;
	}

	if (with_guest_part_info) {
		ret = acrn_prepare_guest_part_info(ctx);
		if (ret)
			return ret;
		vsbl_para->guest_part_info_address = GUEST_PART_INFO_OFF(ctx);
		vsbl_para->guest_part_info_size = guest_part_info_size;
	} else {
		vsbl_para->guest_part_info_address = 0;
		vsbl_para->guest_part_info_size = 0;
	}

	ret = acrn_prepare_vsbl(ctx);
	if (ret)
		return ret;

	vsbl_para->vsbl_address = VSBL_OFF(ctx);
	vsbl_para->vsbl_size = vsbl_size;

	vsbl_para->e820_entries = add_e820_entry(e820, vsbl_para->e820_entries,
		vsbl_para->vsbl_address, vsbl_size, E820_TYPE_RESERVED);

	*vsbl_entry = *((uint32_t *) vsbl_start_addr);

	vsbl_para->boot_device_address = boot_blk_bdf;
	vsbl_para->trusty_enabled = trusty_enabled;

	return 0;
}
