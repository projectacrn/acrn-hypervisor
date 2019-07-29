/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "log_sys.h"

void debug_log(const int level, const char *func, const int line, ...)
{
	va_list args;
	char *fmt;
	char *head;
	char *msg;

	if (level > LOG_LEVEL)
		return;

	va_start(args, line);
	fmt = va_arg(args, char *);
	if (!fmt) {
		va_end(args);
		return;
	}
	if (vasprintf(&msg, fmt, args) == -1) {
		va_end(args);
		return;
	}
	va_end(args);

	if (asprintf(&head, "<%-20s%5d>: ", func, line) == -1) {
		free(msg);
		return;
	}

	sd_journal_print(level, "%s%s", head, msg);
	free(msg);
	free(head);
}
