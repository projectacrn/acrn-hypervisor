/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>

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

		return (*nchars += (s - p));
	}
	/* fill mode */
	else {
		*nchars += sz;
		while (sz--)
			console_putc(*s);
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
	memset(&param, 0, sizeof(param));
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
