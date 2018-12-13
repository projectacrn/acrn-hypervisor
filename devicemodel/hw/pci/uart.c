/*-
 * Copyright (c) 2012 NetApp, Inc.
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
#include <sys/errno.h>

#include "pci_core.h"
#include "uart_core.h"

/*
 * Pick a PCI vid/did of a chip with a single uart at
 * BAR0, that most versions of FreeBSD can understand:
 * Siig CyberSerial 1-port.
 */
#define COM_VENDOR	0x131f
#define COM_DEV		0x2000

static void
pci_uart_intr_assert(void *arg)
{
	struct pci_vdev *dev = arg;

	pci_lintr_assert(dev);
}

static void
pci_uart_intr_deassert(void *arg)
{
	struct pci_vdev *dev = arg;

	pci_lintr_deassert(dev);
}

static void
pci_uart_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
	       int baridx, uint64_t offset, int size, uint64_t value)
{
	assert(baridx == 0);
	assert(size == 1);

	uart_write(dev->arg, offset, value);
}

uint64_t
pci_uart_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
	      int baridx, uint64_t offset, int size)
{
	uint8_t val;

	assert(baridx == 0);
	assert(size == 1);

	val = uart_read(dev->arg, offset);
	return val;
}

static int
pci_uart_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	pci_emul_alloc_bar(dev, 0, PCIBAR_IO, UART_IO_BAR_SIZE);
	pci_lintr_request(dev);

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, COM_DEV);
	pci_set_cfgdata16(dev, PCIR_VENDOR, COM_VENDOR);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_SIMPLECOMM);

	dev->arg = uart_set_backend(pci_uart_intr_assert, pci_uart_intr_deassert, dev, opts);
	if (dev->arg == NULL) {
		fprintf(stderr, "Unable to initialize backend '%s' for "
		    "pci uart at %d:%d\n", opts, dev->slot, dev->func);
		return -1;
	}

	return 0;
}

static void
pci_uart_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct uart_vdev *uart = (struct uart_vdev *)dev->arg;

	if (uart == NULL)
		return;

	uart_release_backend(uart, opts);
}

struct pci_vdev_ops pci_ops_com = {
	.class_name	= "uart",
	.vdev_init	= pci_uart_init,
	.vdev_deinit	= pci_uart_deinit,
	.vdev_barwrite	= pci_uart_write,
	.vdev_barread	= pci_uart_read
};
DEFINE_PCI_DEVTYPE(pci_ops_com);
