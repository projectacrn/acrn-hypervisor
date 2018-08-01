/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <systemd/sd-journal.h>
#include "log_sys.h"

void do_log(const int level,
#ifdef DEBUG_ACRN_CRASHLOG
			const char *func, const int line,
#endif
			...)
{
	va_list args;
	char *fmt;
	char log[MAX_LOG_LEN];
	int n = 0;
#ifdef DEBUG_ACRN_CRASHLOG
	const char header_fmt[] = "<%-20s%5d>: ";
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
	n = snprintf(log, sizeof(log), header_fmt, func, line);
	if (n < 0 || (size_t)n >= sizeof(log))
		n = 0;
#endif
	/* msg */
	vsnprintf(log + n, sizeof(log) - (size_t)n, fmt, args);
	log[sizeof(log) - 1] = 0;
	va_end(args);

	sd_journal_print(level, "%s", log);
}
