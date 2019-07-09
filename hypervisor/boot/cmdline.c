/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <multiboot.h>
#include <pgtable.h>
#include <dbg_cmd.h>
#include <logmsg.h>

#define ACRN_DBG_PARSE		6

int32_t parse_hv_cmdline(void)
{
	const char *start;
	const char *end;
	struct multiboot_info *mbi = NULL;

	if (boot_regs[0] != MULTIBOOT_INFO_MAGIC) {
		return -EINVAL;
	}

	mbi = (struct multiboot_info *)(hpa2hva((uint64_t)boot_regs[1]));
	dev_dbg(ACRN_DBG_PARSE, "Multiboot detected, flag=0x%x", mbi->mi_flags);

	if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE) == 0U) {
		dev_dbg(ACRN_DBG_PARSE, "no hv cmdline!");
		return -EINVAL;
	}

	start = (char *)hpa2hva((uint64_t)mbi->mi_cmdline);
	dev_dbg(ACRN_DBG_PARSE, "hv cmdline: %s", start);

	do {
		while (*start == ' ')
			start++;

		end = start + 1;
		while ((*end != ' ') && ((*end) != '\0'))
			end++;

		if (!handle_dbg_cmd(start, (int32_t)(end - start))) {
			/* if not handled by handle_dbg_cmd, it can be handled further */
		}
		start = end + 1;

	} while (((*end) != '\0') && ((*start) != '\0'));

	return 0;
}
