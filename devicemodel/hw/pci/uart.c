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
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/user.h>

#include "pci_core.h"
#include "uart_core.h"
#include "dm_string.h"
#include "vmmapi.h"

/*
 * Pick a PCI vid/did of a chip with a single uart at
 * BAR0, that most versions of FreeBSD can understand:
 * Siig CyberSerial 1-port.
 */
#define COM_VENDOR	0x9710
#define COM_DEV		0x9900

#define VUART_IDX	"vuart_idx"

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
	if (baridx == 0 && size == 1)
		uart_write(dev->arg, offset, value);
}

uint64_t
pci_uart_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
	      int baridx, uint64_t offset, int size)
{
	uint8_t val = 0xff;

	if (baridx == 0 && size == 1)
		val = uart_read(dev->arg, offset);
	return val;
}

static int
pci_uart_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	char *tmp, *val = NULL;
	bool is_hv_land = false;
	uint32_t vuart_idx;
	struct acrn_vdev vdev = {};
	int32_t err = 0;

	if (opts != NULL) {
		tmp = val= strdup(opts);
		if (!tmp) {
			pr_err("No memory for strdup, can't parse uart args!!!!\n");
		} else {
			if (!strncmp(tmp, VUART_IDX, strlen(VUART_IDX))) {
				 tmp = strsep(&val, ":");
				 if (!val) {
					pr_err("pci vuart miss vuart_idx value!!!!\n");
				 } else {
					if (dm_strtoui(val, &val, 10, &vuart_idx)) {
						pr_err("wrong vuart_idx value!!!!\n");
					} else {
						is_hv_land = true;
					}
				 }
			}
		}
	}

	if (is_hv_land) {
		pci_emul_alloc_bar(dev, 0, PCIBAR_MEM32, 256);
		pci_emul_alloc_bar(dev, 1, PCIBAR_MEM32, PAGE_SIZE);
		dev->arg = NULL;
		vdev.id.fields.vendor = COM_VENDOR;
		vdev.id.fields.device = COM_DEV;
		vdev.slot = PCI_BDF(dev->bus, dev->slot, dev->func);
		vdev.io_addr[0] = pci_get_cfgdata32(dev, PCIR_BAR(0));
		vdev.io_addr[1] = pci_get_cfgdata32(dev, PCIR_BAR(1));
		*((uint32_t *)vdev.args) = vuart_idx;
		err = vm_add_hv_vdev(ctx, &vdev);
		if (err) {
			pr_err("HV can't create vuart with vuart_idx=%d\n", vuart_idx);
		}
	} else {
		pci_emul_alloc_bar(dev, 0, PCIBAR_IO, UART_IO_BAR_SIZE);
		pci_lintr_request(dev);

		/* initialize config space */
		pci_set_cfgdata16(dev, PCIR_DEVICE, COM_DEV);
		pci_set_cfgdata16(dev, PCIR_VENDOR, COM_VENDOR);
		pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_SIMPLECOMM);

		dev->arg = uart_set_backend(pci_uart_intr_assert, pci_uart_intr_deassert, dev, opts);
		if (dev->arg == NULL) {
			pr_err("Unable to initialize backend '%s' for "
			    "pci uart at %d:%d\n", opts, dev->slot, dev->func);
			err = -1;
		}
	}

	return err;
}

static void
pci_uart_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct uart_vdev *uart = (struct uart_vdev *)dev->arg;
	struct acrn_vdev emul_dev = {};

	if (uart == NULL) {
		emul_dev.id.fields.vendor = COM_VENDOR;
		emul_dev.id.fields.device = COM_DEV;
		emul_dev.slot = PCI_BDF(dev->bus, dev->slot, dev->func);
		vm_remove_hv_vdev(ctx, &emul_dev);
		return;
	}

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
