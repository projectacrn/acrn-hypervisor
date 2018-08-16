/*
 * Copyright (C) 2018 Intel Corporation
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "vmmapi.h"
#include "tpm.h"
#include "tpm_internal.h"

static int tpm_debug;
#define LOG_TAG "tpm: "
#define DPRINTF(fmt, args...) \
	do { if (tpm_debug) printf(LOG_TAG "%s:" fmt, __func__, ##args); } while (0)
#define WPRINTF(fmt, args...) \
	do { printf(LOG_TAG "%s:" fmt, __func__, ##args); } while (0)

#define STR_MAX_LEN 1024U
static char *sock_path = NULL;

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
	size_t len = strlen(arg);

	if (len > STR_MAX_LEN)
		return -1;

	if (SOCK_PATH_OPT == getsubopt(&arg, token, &value)) {
		if (value == NULL) {
			DPRINTF("Invalid vtpm socket path\n");
			return -1;
		}
		sock_path = calloc(len + 1, 1);
		if (!sock_path)
			return -1;
		strcpy(sock_path, value);
	}

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
