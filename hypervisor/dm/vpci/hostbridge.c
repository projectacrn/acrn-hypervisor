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

#include <hypervisor.h>
#include "pci_priv.h"

static int vdev_hostbridge_init(struct pci_vdev *vdev)
{
	/* PCI config space */
	pci_vdev_write_cfg_u16(vdev, PCIR_VENDOR, 0x8086U);
	pci_vdev_write_cfg_u16(vdev, PCIR_DEVICE, 0x5af0U);

	pci_vdev_write_cfg_u8(vdev, PCIR_REVID, 0xbU);

	pci_vdev_write_cfg_u8(vdev, PCIR_HDRTYPE, PCIM_HDRTYPE_NORMAL
		| PCIM_MFDEV);
	pci_vdev_write_cfg_u8(vdev, PCIR_CLASS, PCIC_BRIDGE);
	pci_vdev_write_cfg_u8(vdev, PCIR_SUBCLASS, PCIS_BRIDGE_HOST);

	pci_vdev_write_cfg_u8(vdev, 0x34U, 0xe0U);
	pci_vdev_write_cfg_u8(vdev, 0x3cU, 0xe0U);
	pci_vdev_write_cfg_u8(vdev, 0x48U, 0x1U);
	pci_vdev_write_cfg_u8(vdev, 0x4aU, 0xd1U);
	pci_vdev_write_cfg_u8(vdev, 0x4bU, 0xfeU);
	pci_vdev_write_cfg_u8(vdev, 0x50U, 0xc1U);
	pci_vdev_write_cfg_u8(vdev, 0x51U, 0x2U);
	pci_vdev_write_cfg_u8(vdev, 0x54U, 0x33U);
	pci_vdev_write_cfg_u8(vdev, 0x58U, 0x7U);
	pci_vdev_write_cfg_u8(vdev, 0x5aU, 0xf0U);
	pci_vdev_write_cfg_u8(vdev, 0x5bU, 0x7fU);
	pci_vdev_write_cfg_u8(vdev, 0x60U, 0x1U);
	pci_vdev_write_cfg_u8(vdev, 0x63U, 0xe0U);
	pci_vdev_write_cfg_u8(vdev, 0xabU, 0x80U);
	pci_vdev_write_cfg_u8(vdev, 0xacU, 0x2U);
	pci_vdev_write_cfg_u8(vdev, 0xb0U, 0x1U);
	pci_vdev_write_cfg_u8(vdev, 0xb3U, 0x7cU);
	pci_vdev_write_cfg_u8(vdev, 0xb4U, 0x1U);
	pci_vdev_write_cfg_u8(vdev, 0xb6U, 0x80U);
	pci_vdev_write_cfg_u8(vdev, 0xb7U, 0x7bU);
	pci_vdev_write_cfg_u8(vdev, 0xb8U, 0x1U);
	pci_vdev_write_cfg_u8(vdev, 0xbbU, 0x7bU);
	pci_vdev_write_cfg_u8(vdev, 0xbcU, 0x1U);
	pci_vdev_write_cfg_u8(vdev, 0xbfU, 0x80U);
	pci_vdev_write_cfg_u8(vdev, 0xe0U, 0x9U);
	pci_vdev_write_cfg_u8(vdev, 0xe2U, 0xcU);
	pci_vdev_write_cfg_u8(vdev, 0xe3U, 0x1U);
	pci_vdev_write_cfg_u8(vdev, 0xf5U, 0xfU);
	pci_vdev_write_cfg_u8(vdev, 0xf6U, 0x1cU);
	pci_vdev_write_cfg_u8(vdev, 0xf7U, 0x1U);

	return 0;
}

static int vdev_hostbridge_deinit(__unused struct pci_vdev *vdev)
{
	return 0;
}

static int vdev_hostbridge_cfgread(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t *val)
{
	/* Assumption: access needed to be aligned on 1/2/4 bytes */
	if ((offset & (bytes - 1U)) != 0U) {
		*val = 0xFFFFFFFFU;
		return -EINVAL;
	}

	*val = pci_vdev_read_cfg(vdev, offset, bytes);

	return 0;
}

static int vdev_hostbridge_cfgwrite(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t val)
{
	/* Assumption: access needed to be aligned on 1/2/4 bytes */
	if ((offset & (bytes - 1U)) != 0U) {
		return -EINVAL;
	}

	if (!pci_bar_access(offset)) {
		pci_vdev_write_cfg(vdev, offset, bytes, val);
	}

	return 0;
}

struct pci_vdev_ops pci_ops_vdev_hostbridge = {
	.init = vdev_hostbridge_init,
	.deinit = vdev_hostbridge_deinit,
	.cfgread = vdev_hostbridge_cfgread,
	.cfgwrite = vdev_hostbridge_cfgwrite,
};

