/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>

/* FIXME: Temporary RISC-V build workaround
 * This file provides pr_xxx function stubs to satisfy existing
 * code dependencies. Remove this file and migrate to the common
 * logmsg.c implementation once the debug module is properly
 * integrated.
 */
void do_logmsg(uint32_t severity, const char *fmt, ...)
{
	(void)severity;
	(void)fmt;
}
