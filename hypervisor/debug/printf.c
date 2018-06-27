/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static int charout(int cmd, const char *s, int sz, void *hnd)
{
	/* pointer to an integer to store the number of characters */
	int *nchars = (int *)hnd;
	/* working pointer */
	const char *p = s;

	/* copy mode ? */
	if (cmd == PRINT_CMD_COPY) {
		/* copy all characters until NUL is found */
		if (sz < 0)
			s += console_puts(s);

		/* copy 'sz' characters */
		else
			s += console_write(s, sz);

		*nchars += (s - p);
		return *nchars;
	}
	/* fill mode */
	else {
		*nchars += sz;
		while (sz != 0) {
			console_putc(*s);
			sz--;
		}
	}

	return *nchars;
}

int vprintf(const char *fmt, va_list args)
{
	/* struct to store all necessary parameters */
	struct print_param param;
	/* the result of this function */
	int res = 0;
	/* argument fo charout() */
	int nchars = 0;

	/* initialize parameters */
	(void)memset(&param, 0, sizeof(param));
	param.emit = charout;
	param.data = &nchars;

	/* execute the printf() */
	res = do_print(fmt, &param, args);

	/* done */
	return res;
}

int printf(const char *fmt, ...)
{
	/* variable argument list needed for do_print() */
	va_list args;
	/* the result of this function */
	int res;

	va_start(args, fmt);

	/* execute the printf() */
	res = vprintf(fmt, args);

	/* destroy parameter list */
	va_end(args);

	/* done */
	return res;
}
