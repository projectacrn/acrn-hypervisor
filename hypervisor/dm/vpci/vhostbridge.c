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


/*_
* Emulate a PCI Host bridge:
* Intel Corporation Celeron N3350/Pentium N4200/Atom E3900
* Series Host Bridge (rev 0b)
*/

#include <vm.h>
#include <pci.h>
#include "vpci_priv.h"


/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 */
static void init_vhostbridge(struct pci_vdev *vdev)
{
	/* PCI config space */
	pci_vdev_write_vcfg(vdev, PCIR_VENDOR, 2U, 0x8086U);
	pci_vdev_write_vcfg(vdev, PCIR_DEVICE, 2U, 0x5af0U);

	pci_vdev_write_vcfg(vdev, PCIR_REVID, 1U, 0xbU);

	pci_vdev_write_vcfg(vdev, PCIR_HDRTYPE, 1U, (PCIM_HDRTYPE_NORMAL | PCIM_MFDEV));
	pci_vdev_write_vcfg(vdev, PCIR_CLASS, 1U, PCIC_BRIDGE);
	pci_vdev_write_vcfg(vdev, PCIR_SUBCLASS, 1U, PCIS_BRIDGE_HOST);

	pci_vdev_write_vcfg(vdev, 0x34U, 1U, 0xe0U);
	pci_vdev_write_vcfg(vdev, 0x3cU, 1U, 0xe0U);
	pci_vdev_write_vcfg(vdev, 0x48U, 1U, 0x1U);
	pci_vdev_write_vcfg(vdev, 0x4aU, 1U, 0xd1U);
	pci_vdev_write_vcfg(vdev, 0x4bU, 1U, 0xfeU);
	pci_vdev_write_vcfg(vdev, 0x50U, 1U, 0xc1U);
	pci_vdev_write_vcfg(vdev, 0x51U, 1U, 0x2U);
	pci_vdev_write_vcfg(vdev, 0x54U, 1U, 0x33U);
	pci_vdev_write_vcfg(vdev, 0x58U, 1U, 0x7U);
	pci_vdev_write_vcfg(vdev, 0x5aU, 1U, 0xf0U);
	pci_vdev_write_vcfg(vdev, 0x5bU, 1U, 0x7fU);
	pci_vdev_write_vcfg(vdev, 0x60U, 1U, 0x1U);
	pci_vdev_write_vcfg(vdev, 0x63U, 1U, 0xe0U);
	pci_vdev_write_vcfg(vdev, 0xabU, 1U, 0x80U);
	pci_vdev_write_vcfg(vdev, 0xacU, 1U, 0x2U);
	pci_vdev_write_vcfg(vdev, 0xb0U, 1U, 0x1U);
	pci_vdev_write_vcfg(vdev, 0xb3U, 1U, 0x7cU);
	pci_vdev_write_vcfg(vdev, 0xb4U, 1U, 0x1U);
	pci_vdev_write_vcfg(vdev, 0xb6U, 1U, 0x80U);
	pci_vdev_write_vcfg(vdev, 0xb7U, 1U, 0x7bU);
	pci_vdev_write_vcfg(vdev, 0xb8U, 1U, 0x1U);
	pci_vdev_write_vcfg(vdev, 0xbbU, 1U, 0x7bU);
	pci_vdev_write_vcfg(vdev, 0xbcU, 1U, 0x1U);
	pci_vdev_write_vcfg(vdev, 0xbfU, 1U, 0x80U);
	pci_vdev_write_vcfg(vdev, 0xe0U, 1U, 0x9U);
	pci_vdev_write_vcfg(vdev, 0xe2U, 1U, 0xcU);
	pci_vdev_write_vcfg(vdev, 0xe3U, 1U, 0x1U);
	pci_vdev_write_vcfg(vdev, 0xf5U, 1U, 0xfU);
	pci_vdev_write_vcfg(vdev, 0xf6U, 1U, 0x1cU);
	pci_vdev_write_vcfg(vdev, 0xf7U, 1U, 0x1U);

	vdev->parent_user = NULL;
	vdev->user = vdev;
}

static void deinit_vhostbridge(__unused struct pci_vdev *vdev)
{
	vdev->parent_user = NULL;
	vdev->user = NULL;
}


/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 */
static int32_t read_vhostbridge_cfg(const struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t *val)
{
	*val = pci_vdev_read_vcfg(vdev, offset, bytes);
	return 0;
}


/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 */
static int32_t write_vhostbridge_cfg(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t val)
{
	if (!is_bar_offset(PCI_BAR_COUNT, offset)) {
		pci_vdev_write_vcfg(vdev, offset, bytes, val);
	}
	return 0;
}

const struct pci_vdev_ops vhostbridge_ops = {
	.init_vdev	= init_vhostbridge,
	.deinit_vdev	= deinit_vhostbridge,
	.write_vdev_cfg	= write_vhostbridge_cfg,
	.read_vdev_cfg	= read_vhostbridge_cfg,
};
