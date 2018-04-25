/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <systemd/sd-journal.h>
#include "log_sys.h"

void do_log(int level,
#ifdef DEBUG_ACRN_CRASHLOG
			const char *func, int line,
#endif
			...)
{
	va_list args;
	char *fmt;
	char log[MAX_LOG_LEN] = {0};
#ifdef DEBUG_ACRN_CRASHLOG
	char header_fmt[] = "<%-20s%d>: ";
#endif

	if (level > LOG_LEVEL)
		return;

#ifdef DEBUG_ACRN_CRASHLOG
	va_start(args, line);
#else
	va_start(args, level);
#endif
	fmt = va_arg(args, char *);
	if (!fmt)
		return;

#ifdef DEBUG_ACRN_CRASHLOG
	/* header */
	snprintf(log, sizeof(log) - 1, header_fmt, func, line);
#endif
	/* msg */
	vsnprintf(log + strlen(log), sizeof(log) - strlen(log) - 1, fmt, args);
	va_end(args);

	sd_journal_print(level, log);
}
