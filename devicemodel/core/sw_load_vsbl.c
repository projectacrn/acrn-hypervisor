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

#include "dm.h"
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

/*                 vsbl binary layout:
 *
 * +--------------------------------------------------+ <--vSBL Top
 * |             |offset: Top - 0x10 (reset vector)   |
 * + STAGEINIT   |------------------------------------+
 * | (0x10000)   |other                               |
 * +--------------------------------------------------+
 * |                                                  |
 * + PAYLOAD                                          +
 * |(0x100000)                                        |
 * +--------------------------------------------------+
 * |                                                  |
 * + vFastboot                                        +
 * |(0x200000)                                        |
 * +--------------------------------------------------+
 */

/* Check default e820 table in sw_load_common.c for info about ctx->lowmem */
#define	CONFIGPAGE_OFF(ctx)	((ctx)->lowmem - 4*KB)
#define	VSBL_ENTRY_OFF(ctx)	((ctx)->lowmem - 6*KB)
#define	BOOTARGS_OFF(ctx)	((ctx)->lowmem - 8*KB)
#define	E820_TABLE_OFF(ctx)	((ctx)->lowmem - 12*KB)
#define	GUEST_PART_INFO_OFF(ctx)	((ctx)->lowmem - 16*KB)
/* vsbl real entry is reset vector, which is (VSBL_TOP - 16) */
#define	VSBL_TOP(ctx)		(64*MB)

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
	uint32_t		reserved;
	uint32_t		boot_device_address;
};

static char guest_part_info_path[STR_LEN];
static size_t guest_part_info_size;
static bool with_guest_part_info;

static char vsbl_path[STR_LEN];
static size_t vsbl_size;

static int boot_blk_bdf;

extern int init_cmos_vrpmb(struct vmctx *ctx);

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
	int error;
	size_t len = strnlen(arg, STR_LEN);

	if (len < STR_LEN) {
		strncpy(guest_part_info_path, arg, len + 1);
		error = check_image(guest_part_info_path, 0, &guest_part_info_size);
		assert(!error);

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
	long len;
	size_t read;

	fp = fopen(guest_part_info_path, "r");
	if (fp == NULL) {
		fprintf(stderr,
			"SW_LOAD ERR: could not open partition blob %s\n",
			guest_part_info_path);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);

	if (len != guest_part_info_size) {
		fprintf(stderr,
			"SW_LOAD ERR: partition blob changed\n");
		fclose(fp);
		return -1;
	}

	if ((len + GUEST_PART_INFO_OFF(ctx)) > BOOTARGS_OFF(ctx)) {
		fprintf(stderr,
			"SW_LOAD ERR: too large partition blob\n");
		fclose(fp);
		return -1;
	}

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
	printf("SW_LOAD: partition blob %s size %lu copy to guest 0x%lx\n",
		guest_part_info_path, guest_part_info_size,
		GUEST_PART_INFO_OFF(ctx));

	return 0;
}

int
acrn_parse_vsbl(char *arg)
{
	int error;
	size_t len = strnlen(arg, STR_LEN);

	if (len < STR_LEN) {
		strncpy(vsbl_path, arg, len + 1);
		error = check_image(vsbl_path, 8 * MB, &vsbl_size);
		assert(!error);

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
	size_t read;

	fp = fopen(vsbl_path, "r");
	if (fp == NULL) {
		fprintf(stderr,
			"SW_LOAD ERR: could not open vsbl file: %s\n",
			vsbl_path);
		return -1;
	}

	fseek(fp, 0, SEEK_END);

	if (ftell(fp) != vsbl_size) {
		fprintf(stderr,
			"SW_LOAD ERR: vsbl file changed\n");
		fclose(fp);
		return -1;
	}

	fseek(fp, 0, SEEK_SET);
	read = fread(ctx->baseaddr + VSBL_TOP(ctx) - vsbl_size,
		sizeof(char), vsbl_size, fp);
	if (read < vsbl_size) {
		fprintf(stderr,
			"SW_LOAD ERR: could not read whole partition blob\n");
		fclose(fp);
		return -1;
	}
	fclose(fp);
	printf("SW_LOAD: partition blob %s size %lu copy to guest 0x%lx\n",
		vsbl_path, vsbl_size, VSBL_TOP(ctx) - vsbl_size);

	return 0;
}

int
acrn_sw_load_vsbl(struct vmctx *ctx)
{
	int ret;
	struct e820_entry *e820;
	struct vsbl_para *vsbl_para;

	init_cmos_vrpmb(ctx);

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

	vsbl_para->vsbl_address = VSBL_TOP(ctx) - vsbl_size;
	vsbl_para->vsbl_size = vsbl_size;

	vsbl_para->e820_entries = add_e820_entry(e820, vsbl_para->e820_entries,
		vsbl_para->vsbl_address, vsbl_size, E820_TYPE_RESERVED);

	printf("SW_LOAD: vsbl_entry 0x%lx\n", VSBL_TOP(ctx) - 16);

	vsbl_para->boot_device_address = boot_blk_bdf;
	vsbl_para->trusty_enabled = trusty_enabled;

	/* set guest bsp state. Will call hypercall set bsp state
	 * after bsp is created.
	 */
	memset(&ctx->bsp_regs, 0, sizeof( struct acrn_set_vcpu_regs));
	ctx->bsp_regs.vcpu_id = 0;

	/* CR0_ET | CR0_NE */
	ctx->bsp_regs.vcpu_regs.cr0 = 0x30U;
	ctx->bsp_regs.vcpu_regs.cs_ar = 0x009FU;
	ctx->bsp_regs.vcpu_regs.cs_sel = 0xF000U;
	ctx->bsp_regs.vcpu_regs.cs_limit = 0xFFFFU;
	ctx->bsp_regs.vcpu_regs.cs_base = (VSBL_TOP(ctx) - 16) &0xFFFF0000UL;
	ctx->bsp_regs.vcpu_regs.rip = (VSBL_TOP(ctx) - 16) & 0xFFFFUL;
	ctx->bsp_regs.vcpu_regs.gprs.rsi = CONFIGPAGE_OFF(ctx);

	return 0;
}
