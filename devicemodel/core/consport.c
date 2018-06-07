/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>

#include "inout.h"
#include "lpc.h"

#define	BVM_CONSOLE_PORT	0x220
#define	BVM_CONS_SIG		('b' << 8 | 'v')

static bool bvmcons_enabled = false;
static struct termios tio_orig, tio_new;

static void
ttyclose(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &tio_orig);
}

static void
ttyopen(void)
{
	tcgetattr(STDIN_FILENO, &tio_orig);

	cfmakeraw(&tio_new);
	tcsetattr(STDIN_FILENO, TCSANOW, &tio_new);

	atexit(ttyclose);
}

static bool
tty_char_available(void)
{
	fd_set rfds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0)
		return true;
	else
		return false;
}

static int
ttyread(void)
{
	char rb;

	if (tty_char_available()) {
		if (read(STDIN_FILENO, &rb, 1) > 0)
			return (rb & 0xff);
	}
	return -1;
}


static	int
ttywrite(unsigned char wb)
{
	if (write(STDOUT_FILENO, &wb, 1) > 0)
		return 1;

	return -1;
}

static int
console_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		uint32_t *eax, void *arg)
{
	static int opened;

	if (bytes == 2 && in) {
		*eax = BVM_CONS_SIG;
		return 0;
	}

	/*
	 * Guests might probe this port to look for old ISA devices
	 * using single-byte reads.  Return 0xff for those.
	 */
	if (bytes == 1 && in) {
		*eax = 0xff;
		return 0;
	}

	if (bytes != 4)
		return -1;

	if (!opened) {
		ttyopen();
		opened = 1;
	}

	if (in)
		*eax = ttyread();
	else
		ttywrite(*eax);

	return 0;
}

SYSRES_IO(BVM_CONSOLE_PORT, 4);

static struct inout_port consport = {
	"bvmcons",
	BVM_CONSOLE_PORT,
	1,
	IOPORT_F_INOUT,
	console_handler
};

void
enable_bvmcons(void)
{
	bvmcons_enabled = true;
}

int
init_bvmcons(void)
{
	if (bvmcons_enabled)
		register_inout(&consport);

	return 0;
}

void
deinit_bvmcons(void)
{
	if (bvmcons_enabled)
		unregister_inout(&consport);
}
