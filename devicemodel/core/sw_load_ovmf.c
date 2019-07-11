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

#include "dm.h"
#include "vmmapi.h"
#include "sw_load.h"


/*                 ovmf binary layout:
 *
 * +--------------------------------------------------+ <--OVMF Top
 * |             |offset: Top - 0x10 (reset vector)   |
 * + SECFV       |------------------------------------+
 * |             |other                               |
 * +--------------------------------------------------+
 * |                                                  |
 * + FVMAIN_COMPACT                                   +
 * |                                                  |
 * +--------------------------------------------------+
 * |                                                  |
 * + NV data storage                                  +
 * |                                                  |
 * +--------------------------------------------------+ <--OVMF offset 0
 */

/* ovmf real entry is reset vector, which is (OVMF_TOP - 16) */
#define OVMF_TOP(ctx)		(4*GB)

/* ovmf NV storage begins at offset 0 */
#define OVMF_NVSTORAGE_OFFSET (OVMF_TOP(ctx) - ovmf_size)

/* ovmf image size limit */
#define OVMF_SZ_LIMIT		(2*MB)

/* ovmf NV storage size */
#define OVMF_NVSTORAGE_SZ	(128*KB)

/* located in the ROM area */
#define OVMF_E820_BASE		0x000EF000UL

static char ovmf_path[STR_LEN];
static size_t ovmf_size;
bool writeback_nv_storage;

extern int init_cmos_vrpmb(struct vmctx *ctx);

size_t
ovmf_image_size(void)
{
	return ovmf_size;
}

int
acrn_parse_ovmf(char *arg)
{
	int error = -1;
	char *str, *cp, *token = NULL;
	size_t len = strnlen(arg, STR_LEN);

	str = strdup(arg);
	if (len < STR_LEN) {
		cp = str;
		token = strsep(&cp, ",");
		while (token != NULL) {
			if (strcmp(token, "w") == 0) {
				writeback_nv_storage = true;
			} else {
				len = strnlen(token, STR_LEN);
				strncpy(ovmf_path, token, len + 1);
				if (check_image(ovmf_path, OVMF_SZ_LIMIT, &ovmf_size) != 0)
					break;
				ovmf_file_name = ovmf_path;
				printf("SW_LOAD: get ovmf path %s, size 0x%lx\n",
					ovmf_path, ovmf_size);
				error = 0;
			}
			token = strsep(&cp, ",");
		}
	}
	free(str);
	return error;
}

static int
acrn_prepare_ovmf(struct vmctx *ctx)
{
	FILE *fp;
	size_t read;

	fp = fopen(ovmf_path, "r");
	if (fp == NULL) {
		fprintf(stderr,
			"SW_LOAD ERR: could not open ovmf file: %s\n",
			ovmf_path);
		return -1;
	}

	fseek(fp, 0, SEEK_END);

	if (ftell(fp) != ovmf_size) {
		fprintf(stderr,
			"SW_LOAD ERR: ovmf file changed\n");
		fclose(fp);
		return -1;
	}

	fseek(fp, 0, SEEK_SET);
	read = fread(ctx->baseaddr + OVMF_TOP(ctx) - ovmf_size,
		sizeof(char), ovmf_size, fp);

	if (read < ovmf_size) {
		fprintf(stderr,
			"SW_LOAD ERR: could not read whole partition blob\n");
		fclose(fp);
		return -1;
	}

	fclose(fp);
	printf("SW_LOAD: partition blob %s size %lu copy to guest 0x%lx\n",
		ovmf_path, ovmf_size, OVMF_TOP(ctx) - ovmf_size);
	return 0;
}

int
acrn_sw_load_ovmf(struct vmctx *ctx)
{
	int ret;
	struct {
		char signature[4];
		uint32_t nentries;
		struct e820_entry map[];
	} __attribute__((packed)) *e820;

	init_cmos_vrpmb(ctx);

	ret = acrn_prepare_ovmf(ctx);

	if (ret)
		return ret;

	e820 = paddr_guest2host(ctx, OVMF_E820_BASE,
			e820_default_entries[LOWRAM_E820_ENTRY].baseaddr -
			OVMF_E820_BASE);
	if (e820 == NULL)
		return -1;

	strncpy(e820->signature, "820", sizeof(e820->signature));
	e820->nentries = acrn_create_e820_table(ctx, e820->map);

	printf("SW_LOAD: ovmf_entry 0x%lx\n", OVMF_TOP(ctx) - 16);

	/* set guest bsp state. Will call hypercall set bsp state
	 * after bsp is created.
	 */
	memset(&ctx->bsp_regs, 0, sizeof(struct acrn_set_vcpu_regs));
	ctx->bsp_regs.vcpu_id = 0;

	/* CR0_ET | CR0_NE */
	ctx->bsp_regs.vcpu_regs.cr0 = 0x30U;
	ctx->bsp_regs.vcpu_regs.cs_ar = 0x009FU;
	ctx->bsp_regs.vcpu_regs.cs_sel = 0xF000U;
	ctx->bsp_regs.vcpu_regs.cs_limit = 0xFFFFU;
	ctx->bsp_regs.vcpu_regs.cs_base = (OVMF_TOP(ctx) - 16) & 0xFFFF0000UL;
	ctx->bsp_regs.vcpu_regs.rip = (OVMF_TOP(ctx) - 16) & 0xFFFFUL;

	return 0;
}

/* The NV data section is the first 128KB in the OVMF image. At runtime,
 * it's copied into guest memory and behave as RAM to OVMF. It can be
 * accessed and updated by OVMF. To preserve NV section (referred to
 * as Non-Volatile Data Store section in the OVMF spec), we're flushing
 * in-memory data back to the NV data section of the OVMF image file
 * at designated points.
 */
int
acrn_writeback_ovmf_nvstorage(struct vmctx *ctx)
{
	FILE *fp;
	size_t write;

	if (!writeback_nv_storage)
		return 0;

	fp = fopen(ovmf_path, "r+");
	if (fp == NULL) {
		fprintf(stderr,
			"OVMF_WRITEBACK ERR: could not open ovmf file: %s\n",
			ovmf_path);
		return -1;
	}

	fseek(fp, 0, SEEK_END);

	if (ftell(fp) != ovmf_size) {
		fprintf(stderr,
			"SW_LOAD ERR: ovmf file changed\n");
		fclose(fp);
		return -1;
	}

	fseek(fp, 0, SEEK_SET);
	write = fwrite(ctx->baseaddr + OVMF_NVSTORAGE_OFFSET,
		sizeof(char), OVMF_NVSTORAGE_SZ, fp);

	if (write < OVMF_NVSTORAGE_SZ) {
		fprintf(stderr,
			"OVMF_WRITEBACK ERR: could not write back OVMF\n");
		fclose(fp);
		return -1;
	}

	fclose(fp);
	printf("OVMF_WRITEBACK: OVMF has been written back \
		to partition blob %s size %lu from guest 0x%lx\n",
		ovmf_path, OVMF_NVSTORAGE_SZ, OVMF_NVSTORAGE_OFFSET);

	return 0;
}
