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
#include <string.h>
#include <assert.h>

#include "inout.h"

SET_DECLARE(inout_port_set, struct inout_port);

#define	MAX_IOPORTS	(1 << 16)

#define	VERIFY_IOPORT(port, size) \
	((port) >= 0 && (size) > 0 && ((port) + (size)) <= MAX_IOPORTS)

static struct {
	const char	*name;
	int		flags;
	inout_func_t	handler;
	void		*arg;
} inout_handlers[MAX_IOPORTS];

static int
default_inout(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
	      uint32_t *eax, void *arg)
{
	if (in) {
		switch (bytes) {
		case 4:
			*eax = 0xffffffff;
			break;
		case 2:
			*eax = 0xffff;
			break;
		case 1:
			*eax = 0xff;
			break;
		}
	}

	return 0;
}

static void
register_default_iohandler(int start, int size)
{
	struct inout_port iop;

	if (!VERIFY_IOPORT(start, size)) {
		printf("invalid input: port:0x%x, size:%d", start, size);
		return;
	}

	bzero(&iop, sizeof(iop));
	iop.name = "default";
	iop.port = start;
	iop.size = size;
	iop.flags = IOPORT_F_INOUT | IOPORT_F_DEFAULT;
	iop.handler = default_inout;

	register_inout(&iop);
}

int
emulate_inout(struct vmctx *ctx, int *pvcpu, struct pio_request *pio_request)
{
	int bytes, flags, in, port;
	inout_func_t handler;
	void *arg;
	int retval;

	bytes = pio_request->size;
	in = (pio_request->direction == REQUEST_READ);
	port = pio_request->address;

	assert(port + bytes - 1 < MAX_IOPORTS);
	assert(bytes == 1 || bytes == 2 || bytes == 4);

	handler = inout_handlers[port].handler;
	flags = inout_handlers[port].flags;
	arg = inout_handlers[port].arg;

	if (pio_request->direction == REQUEST_READ) {
		if (!(flags & IOPORT_F_IN))
			return -1;
	} else {
		if (!(flags & IOPORT_F_OUT))
			return -1;
	}
	retval = handler(ctx, *pvcpu, in, port, bytes,
		(uint32_t *)&(pio_request->value), arg);
	return retval;
}

void
init_inout(void)
{
	struct inout_port **iopp, *iop;

	/*
	 * Set up the default handler for all ports
	 */
	register_default_iohandler(0, MAX_IOPORTS);

	/*
	 * Overwrite with specified handlers
	 */
	SET_FOREACH(iopp, inout_port_set) {
		iop = *iopp;
		assert(iop->port < MAX_IOPORTS);
		inout_handlers[iop->port].name = iop->name;
		inout_handlers[iop->port].flags = iop->flags;
		inout_handlers[iop->port].handler = iop->handler;
		inout_handlers[iop->port].arg = NULL;
	}
}

int
register_inout(struct inout_port *iop)
{
	int i;

	if (!VERIFY_IOPORT(iop->port, iop->size)) {
		printf("invalid input: port:0x%x, size:%d",
				iop->port, iop->size);
		return -1;
	}

	/*
	 * Verify that the new registration is not overwriting an already
	 * allocated i/o range.
	 */
	if ((iop->flags & IOPORT_F_DEFAULT) == 0) {
		for (i = iop->port; i < iop->port + iop->size; i++) {
			if ((inout_handlers[i].flags & IOPORT_F_DEFAULT) == 0)
				return -1;
		}
	}

	for (i = iop->port; i < iop->port + iop->size; i++) {
		inout_handlers[i].name = iop->name;
		inout_handlers[i].flags = iop->flags;
		inout_handlers[i].handler = iop->handler;
		inout_handlers[i].arg = iop->arg;
	}

	return 0;
}

int
unregister_inout(struct inout_port *iop)
{

	if (!VERIFY_IOPORT(iop->port, iop->size)) {
		printf("invalid input: port:0x%x, size:%d",
				iop->port, iop->size);
		return -1;
	}

	assert(inout_handlers[iop->port].name == iop->name);

	register_default_iohandler(iop->port, iop->size);

	return 0;
}
