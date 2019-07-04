/*-
 * Copyright (c) 2018 Intel
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "vmmapi.h"
#include "inout.h"
#include "mevent.h"

#define DEBUG_IO_BASE	(0xf4)
#define	DEBUG_IO_SIZE	(1)

static int
debugexit_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
	      uint32_t *eax, void *arg)
{
	if (in)
		*eax = 0xFFFF;
	else {
		vm_suspend(ctx, VM_SUSPEND_POWEROFF);
		mevent_notify();
	}
	return 0;
}

void
init_debugexit(void)
{
	struct inout_port iop;

	memset(&iop, 0, sizeof(struct inout_port));
	iop.name = "debugexit";
	iop.port = DEBUG_IO_BASE;
	iop.size = DEBUG_IO_SIZE;
	iop.flags = IOPORT_F_INOUT;
	iop.handler = debugexit_handler;

	register_inout(&iop);
}

void
deinit_debugexit(void)
{
	struct inout_port iop;

	memset(&iop, 0, sizeof(struct inout_port));
	iop.name = "debugexit";
	iop.port = DEBUG_IO_BASE;
	iop.size = DEBUG_IO_SIZE;

	unregister_inout(&iop);
}
