/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <rtl.h>
#include <util.h>
#include <sprintf.h>
#include <console.h>

static void
charout(size_t cmd, const char *s_arg, uint32_t sz_arg, struct snprint_param *param)
{
	const char *s = s_arg;
	uint32_t sz = sz_arg;
	/* pointer to an integer to store the number of characters */
	size_t nchars = param->wrtn;
	/* working pointer */
	const char *p = s;
	size_t len;

	/* copy mode ? */
	if (cmd == PRINT_CMD_COPY) {
		if (sz > 0U) { /* copy 'sz' characters */
			len = console_write(s, sz);
			s += len;
		}

		nchars += (s - p);
	} else {
		/* fill mode */
		nchars += sz;
		while (sz != 0U) {
			console_putc(s);
			sz--;
		}
	}
	param->wrtn = nchars;
}

void vprintf(const char *fmt, va_list args)
{
	/* struct to store all necessary parameters */
	struct print_param param;
	struct snprint_param snparam;

	/* initialize parameters */
	(void)memset(&snparam, 0U, sizeof(snparam));
	(void)memset(&param, 0U, sizeof(param));
	param.emit = charout;
	param.data = &snparam;

	/* execute the printf() */
	do_print(fmt, &param, args);
}

void printf(const char *fmt, ...)
{
	/* variable argument list needed for do_print() */
	va_list args;

	va_start(args, fmt);

	/* execute the printf() */
	vprintf(fmt, args);

	/* destroy parameter list */
	va_end(args);
}
