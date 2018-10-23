/*-
* Copyright (c) 2011 NetApp, Inc.
* Copyright (c) 2018 Intel Corporation
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

#ifndef VPCI_H_
#define VPCI_H_

#include <pci.h>

struct pci_vdev;
struct pci_vdev_ops {
	int (*init)(struct pci_vdev *vdev);

	int (*deinit)(struct pci_vdev *vdev);

	int (*cfgwrite)(struct pci_vdev *vdev, uint32_t offset,
		uint32_t bytes, uint32_t val);

	int (*cfgread)(struct pci_vdev *vdev, uint32_t offset,
		uint32_t bytes, uint32_t *val);
};

struct pci_bar {
	uint64_t base;
	uint64_t size;
	enum pci_bar_type type;
};

struct pci_pdev {
	/* The bar info of the physical PCI device. */
	struct pci_bar bar[PCI_BAR_COUNT];

	/* The bus/device/function triple of the physical PCI device. */
	union pci_bdf bdf;
};

struct pci_vdev {
	struct pci_vdev_ops *ops;
	struct vpci *vpci;
	/* The bus/device/function triple of the virtual PCI device. */
	union pci_bdf vbdf;

	struct pci_pdev pdev;

	uint8_t cfgdata[PCI_REGMAX + 1U];

	/* The bar info of the virtual PCI device. */
	struct pci_bar bar[PCI_BAR_COUNT];
};

struct pci_addr_info {
	union pci_bdf cached_bdf;
	uint32_t cached_reg, cached_enable;
};

struct vpci {
	struct vm *vm;
	struct pci_addr_info addr_info;
};

extern struct pci_vdev_ops pci_ops_vdev_hostbridge;
extern struct pci_vdev_ops pci_ops_vdev_pt;

void vpci_init(struct vm *vm);
void vpci_cleanup(struct vm *vm);

#endif /* VPCI_H_ */
