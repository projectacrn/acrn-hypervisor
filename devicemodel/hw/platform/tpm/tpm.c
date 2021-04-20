/*
 * Copyright (C) 2018 Intel Corporation
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "vmmapi.h"
#include "tpm.h"
#include "tpm_internal.h"
#include "log.h"
#include "mmio_dev.h"
#include "dm.h"

static int tpm_debug;
#define LOG_TAG "tpm: "
#define DPRINTF(fmt, args...) \
	do { if (tpm_debug) pr_dbg(LOG_TAG "%s:" fmt, __func__, ##args); } while (0)
#define WPRINTF(fmt, args...) \
	do { pr_err(LOG_TAG "%s:" fmt, __func__, ##args); } while (0)

#define STR_MAX_LEN 1024U
static char *sock_path = NULL;
static uint32_t vtpm_crb_mmio_addr = 0U;

uint32_t get_vtpm_crb_mmio_addr(void) {
	return vtpm_crb_mmio_addr;
}

uint32_t get_tpm_crb_mmio_addr(void)
{
	uint32_t base;

	if (pt_tpm2) {
		base = (uint32_t)get_mmio_dev_tpm2_base_gpa();
	} else {
		base = get_vtpm_crb_mmio_addr();
	}

	return base;
}


enum {
	SOCK_PATH_OPT = 0
};

char *const token[] = {
	[SOCK_PATH_OPT] = "sock_path",
	NULL
};

int acrn_parse_vtpm2(char *arg)
{
	char *value;
	size_t len = strnlen(arg, STR_MAX_LEN);

	if (len == STR_MAX_LEN)
		return -1;

	if (SOCK_PATH_OPT == getsubopt(&arg, token, &value)) {
		if (value == NULL) {
			DPRINTF("Invalid vtpm socket path\n");
			return -1;
		}
		sock_path = calloc(len + 1, 1);
		if (!sock_path)
			return -1;
		strncpy(sock_path, value, len + 1);
	}
	vtpm2 = true;

	return 0;
}

void init_vtpm2(struct vmctx *ctx)
{
	if (!sock_path) {
		WPRINTF("Invalid socket path!\n");
		return;
	}

	if (init_tpm_emulator(sock_path) < 0) {
		WPRINTF("Failed init tpm emulator!\n");
		return;
	}

	if (mmio_dev_alloc_gpa_resource32(&vtpm_crb_mmio_addr, TPM_CRB_MMIO_SIZE) < 0) {
		WPRINTF("Failed allocate gpa resorce!\n");
		return;
	}

	if (init_tpm_crb(ctx) < 0) {
		WPRINTF("Failed init tpm emulator!\n");
	}
}

void deinit_vtpm2(struct vmctx *ctx)
{
	if (ctx->tpm_dev) {
		deinit_tpm_crb(ctx);

		deinit_tpm_emulator();
	}
}
