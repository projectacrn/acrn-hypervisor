/*
 * Copyright (C) 2018-2020 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <boot.h>
#include <pgtable.h>
#include <dbg_cmd.h>

void parse_hv_cmdline(void)
{
	const char *start = NULL, *end = NULL;
	struct acrn_multiboot_info *mbi = &acrn_mbi;

	if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE) != 0U) {
		start = mbi->mi_cmdline;
	}

	while ((start != NULL) && ((*start) != '\0')) {
		while ((*start) == ' ')
			start++;
		if ((*start) != '\0') {
			end = start + 1;
			while ((*end != ' ') && ((*end) != '\0'))
				end++;

			if (!handle_dbg_cmd(start, (int32_t)(end - start))) {
				/* if not handled by handle_dbg_cmd, it can be handled further */
			}
			start = end;
		}
	}

}
