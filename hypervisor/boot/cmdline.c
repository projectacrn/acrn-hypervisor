/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <boot.h>
#include <pgtable.h>
#include <dbg_cmd.h>
#include <logmsg.h>

void parse_hv_cmdline(void)
{
	const char *start = NULL;
	const char *end = NULL;

	if (boot_from_multiboot1()) {
		struct multiboot_info *mbi = (struct multiboot_info *)(hpa2hva_early((uint64_t)boot_regs[1]));

		if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE) != 0U) {
			start = (char *)hpa2hva_early((uint64_t)mbi->mi_cmdline);
		}
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
