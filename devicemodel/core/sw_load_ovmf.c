/*-
 * Copyright (c) 2018-2022 Intel Corporation.
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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "dm.h"
#include "vmmapi.h"
#include "sw_load.h"
#include "log.h"


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

/* ovmf image size limit */
#define OVMF_SZ_LIMIT		(2*MB)

/* ovmf split images size limit */
#define OVMF_VARS_SZ_LIMIT	(128*KB)
#define OVMF_CODE_SZ_LIMIT	(OVMF_SZ_LIMIT - OVMF_VARS_SZ_LIMIT)

/* ovmf NV storage begins at offset 0 */
#define OVMF_NVSTORAGE_OFFSET	(OVMF_TOP(ctx) - ovmf_image_size())

/* ovmf NV storage size */
#define OVMF_NVSTORAGE_SZ	(ovmf_file_name ? OVMF_VARS_SZ_LIMIT : ovmf_vars_size)

/* located in the ROM area */
#define OVMF_E820_BASE		0x000EF000UL

static char ovmf_path[STR_LEN];
static char ovmf_code_path[STR_LEN];
static char ovmf_vars_path[STR_LEN];
static size_t ovmf_size;
static size_t ovmf_code_size;
static size_t ovmf_vars_size;
static char *mmap_vars;
static bool writeback_nv_storage;

extern int init_cmos_vrpmb(struct vmctx *ctx);

size_t
ovmf_image_size(void)
{
	size_t size = 0;

	if (ovmf_file_name)
		size = ovmf_size;
	else if (ovmf_code_file_name && ovmf_vars_file_name)
		size = ovmf_code_size + ovmf_vars_size;

	return size;
}

int
acrn_parse_ovmf(char *arg)
{
	int error = -1;
	char *str, *cp, *token;

	if (strnlen(arg, STR_LEN) < STR_LEN) {
		str = cp = strdup(arg);

		while ((token = strsep(&cp, ",")) != NULL) {
			if (!strcmp(token, "w")) {
				writeback_nv_storage = true;
			} else if (!strncmp(token, "code=", sizeof("code=") - 1)) {
				token += sizeof("code=") - 1;
				strncpy(ovmf_code_path, token, sizeof(ovmf_code_path));
				if (check_image(ovmf_code_path, OVMF_CODE_SZ_LIMIT,
						&ovmf_code_size) != 0)
					break;
				ovmf_code_file_name = ovmf_code_path;
				pr_notice("SW_LOAD: get ovmf code path %s, size 0x%lx\n",
					ovmf_code_path, ovmf_code_size);
			} else if (!strncmp(token, "vars=", sizeof("vars=") - 1)) {
				token += sizeof("vars=") - 1;
				strncpy(ovmf_vars_path, token, sizeof(ovmf_vars_path));
				if (check_image(ovmf_vars_path, OVMF_VARS_SZ_LIMIT,
						&ovmf_vars_size) != 0)
					break;
				ovmf_vars_file_name = ovmf_vars_path;
				pr_notice("SW_LOAD: get ovmf vars path %s, size 0x%lx\n",
					ovmf_vars_path, ovmf_vars_size);
			} else {
				strncpy(ovmf_path, token, sizeof(ovmf_path));
				if (check_image(ovmf_path, OVMF_SZ_LIMIT, &ovmf_size) != 0)
					break;
				ovmf_file_name = ovmf_path;
				pr_notice("SW_LOAD: get ovmf path %s, size 0x%lx\n",
					ovmf_path, ovmf_size);
			}
		}
		free(str);
	}

	if ((ovmf_file_name != NULL) ^ (ovmf_code_file_name && ovmf_vars_file_name))
		error = 0;

	return error;
}

static int
acrn_prepare_ovmf(struct vmctx *ctx)
{
	int i, flags, fd;
	char *path, *addr;
	size_t size, size_limit, cur_size, read;
	struct flock fl;
	FILE *fp;

	if (ovmf_file_name) {
		path = ovmf_file_name;
		size = ovmf_size;
		size_limit = OVMF_SZ_LIMIT;
	} else {
		path = ovmf_vars_file_name;
		size = ovmf_vars_size;
		size_limit = OVMF_VARS_SZ_LIMIT;
	}

	flags = writeback_nv_storage ? O_RDWR : O_RDONLY;
	addr = ctx->baseaddr + OVMF_TOP(ctx) - ovmf_image_size();

	for (i = 0; i < 2; i++) {
		fd = open(path, flags);

		if (fd == -1) {
			pr_err("SW_LOAD ERR: could not open ovmf file: %s (%s)\n",
				path, strerror(errno));
			return -1;
		}

		/* acquire read lock over the entire file */
		memset(&fl, 0, sizeof(fl));
		fl.l_type = F_RDLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 0;

		if (fcntl(fd, F_SETLK, &fl)) {
			pr_err("SW_LOAD ERR: could not fcntl(F_RDLCK) "
				"ovmf file: %s (%s)\n",
				path, strerror(errno));
			close(fd);
			return -1;
		}

		if (check_image(path, size_limit, &cur_size) != 0) {
			close(fd);
			return -1;
		}

		if (cur_size != size) {
			pr_err("SW_LOAD ERR: ovmf file %s changed\n", path);
			close(fd);
			return -1;
		}

		if (flags == O_RDWR) {
			/* upgrade to write lock */
			memset(&fl, 0, sizeof(fl));
			fl.l_type = F_WRLCK;
			fl.l_whence = SEEK_SET;
			fl.l_start = 0;
			fl.l_len = OVMF_NVSTORAGE_SZ;

			if (fcntl(fd, F_SETLK, &fl)) {
				pr_err("SW_LOAD ERR: could not fcntl(F_WRLCK) "
					"ovmf file: %s (%s)\n",
					path, strerror(errno));
				close(fd);
				return -1;
			}

			mmap_vars = mmap(NULL, OVMF_NVSTORAGE_SZ, PROT_WRITE,
					MAP_SHARED, fd, 0);

			if (mmap_vars == MAP_FAILED) {
				pr_err("SW_LOAD ERR: could not mmap "
					"ovmf file: %s (%s)\n",
					path, strerror(errno));
				close(fd);
				return -1;
			}
		}

		fp = fdopen(fd, "r");

		if (fp == NULL) {
			pr_err("SW_LOAD ERR: could not fdopen "
				"ovmf file: %s (%s)\n",
				path, strerror(errno));
			close(fd);
			return -1;
		}

		fseek(fp, 0, SEEK_SET);
		read = fread(addr, sizeof(char), size, fp);
		fclose(fp);

		if (read < size) {
			pr_err("SW_LOAD ERR: could not read whole partition blob %s\n",
				path);
			return -1;
		}

		pr_info("SW_LOAD: partition blob %s size 0x%lx copied to addr %p\n",
			path, size, addr);

		if (!ovmf_file_name) {
			addr += size;
			path = ovmf_code_file_name;
			size = ovmf_code_size;
			size_limit = OVMF_CODE_SZ_LIMIT;
			flags = O_RDONLY;
		} else
			break;
	}

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

	pr_info("SW_LOAD: ovmf_entry 0x%lx\n", OVMF_TOP(ctx) - 16);

	/* set guest bsp state. Will call hypercall set bsp state
	 * after bsp is created.
	 */
	memset(&ctx->bsp_regs, 0, sizeof(struct acrn_vcpu_regs));
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

/*
 * The NV data section is the first 128KB in the OVMF image. At runtime,
 * it's copied into guest memory and behave as RAM to OVMF. It can be
 * accessed and updated by OVMF. To preserve NV section (referred to
 * as Non-Volatile Data Store section in the OVMF spec), we're flushing
 * in-memory data back to the NV data section of the OVMF image file
 * at designated points.
 */
int
acrn_writeback_ovmf_nvstorage(struct vmctx *ctx)
{
	int i, fd, ret = 0;
	char *path;
	struct flock fl;

	if (!writeback_nv_storage)
		return 0;

	memcpy(mmap_vars, ctx->baseaddr + OVMF_NVSTORAGE_OFFSET,
		OVMF_NVSTORAGE_SZ);

	if (munmap(mmap_vars, OVMF_NVSTORAGE_SZ)) {
		pr_err("SW_LOAD ERR: could not munmap (%s)\n",
			strerror(errno));
		ret = -1;
	}

	mmap_vars = NULL;

	path = ovmf_file_name ? ovmf_file_name : ovmf_vars_file_name;
	pr_info("OVMF_WRITEBACK: OVMF has been written back "
		"to partition blob %s size 0x%lx @ gpa %p\n",
		path, OVMF_NVSTORAGE_SZ, (void *)OVMF_NVSTORAGE_OFFSET);

	for (i = 0; i < 2; i++) {
		fd = open(path, O_RDONLY);

		if (fd == -1) {
			pr_err("SW_LOAD ERR: could not open ovmf file: %s (%s)\n",
				path, strerror(errno));
			ret = -1;
			goto next;
		}

		/* unlock the entire file */
		memset(&fl, 0, sizeof(fl));
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 0;

		if (fcntl(fd, F_SETLK, &fl)) {
			pr_err("SW_LOAD ERR: could not fcntl(F_UNLCK) "
				"ovmf file: %s (%s)\n",
				path, strerror(errno));
			ret = -1;
		}

		close(fd);

next:
		if (!ovmf_file_name)
			path = ovmf_code_file_name;
		else
			break;
	}

	return ret;
}
