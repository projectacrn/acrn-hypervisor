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

void vdev_hostbridge_init(struct pci_vdev *vdev)
{
	/* PCI config space */
	pci_vdev_write_cfg_u16(vdev, PCIR_VENDOR, (uint16_t)0x8086U);
	pci_vdev_write_cfg_u16(vdev, PCIR_DEVICE, (uint16_t)0x5af0U);

	pci_vdev_write_cfg_u8(vdev, PCIR_REVID, (uint8_t)0xbU);

	pci_vdev_write_cfg_u8(vdev, PCIR_HDRTYPE, (uint8_t)PCIM_HDRTYPE_NORMAL
		| PCIM_MFDEV);
	pci_vdev_write_cfg_u8(vdev, PCIR_CLASS, (uint8_t)PCIC_BRIDGE);
	pci_vdev_write_cfg_u8(vdev, PCIR_SUBCLASS, (uint8_t)PCIS_BRIDGE_HOST);

	pci_vdev_write_cfg_u8(vdev, 0x34U, (uint8_t)0xe0U);
	pci_vdev_write_cfg_u8(vdev, 0x3cU, (uint8_t)0xe0U);
	pci_vdev_write_cfg_u8(vdev, 0x48U, (uint8_t)0x1U);
	pci_vdev_write_cfg_u8(vdev, 0x4aU, (uint8_t)0xd1U);
	pci_vdev_write_cfg_u8(vdev, 0x4bU, (uint8_t)0xfeU);
	pci_vdev_write_cfg_u8(vdev, 0x50U, (uint8_t)0xc1U);
	pci_vdev_write_cfg_u8(vdev, 0x51U, (uint8_t)0x2U);
	pci_vdev_write_cfg_u8(vdev, 0x54U, (uint8_t)0x33U);
	pci_vdev_write_cfg_u8(vdev, 0x58U, (uint8_t)0x7U);
	pci_vdev_write_cfg_u8(vdev, 0x5aU, (uint8_t)0xf0U);
	pci_vdev_write_cfg_u8(vdev, 0x5bU, (uint8_t)0x7fU);
	pci_vdev_write_cfg_u8(vdev, 0x60U, (uint8_t)0x1U);
	pci_vdev_write_cfg_u8(vdev, 0x63U, (uint8_t)0xe0U);
	pci_vdev_write_cfg_u8(vdev, 0xabU, (uint8_t)0x80U);
	pci_vdev_write_cfg_u8(vdev, 0xacU, (uint8_t)0x2U);
	pci_vdev_write_cfg_u8(vdev, 0xb0U, (uint8_t)0x1U);
	pci_vdev_write_cfg_u8(vdev, 0xb3U, (uint8_t)0x7cU);
	pci_vdev_write_cfg_u8(vdev, 0xb4U, (uint8_t)0x1U);
	pci_vdev_write_cfg_u8(vdev, 0xb6U, (uint8_t)0x80U);
	pci_vdev_write_cfg_u8(vdev, 0xb7U, (uint8_t)0x7bU);
	pci_vdev_write_cfg_u8(vdev, 0xb8U, (uint8_t)0x1U);
	pci_vdev_write_cfg_u8(vdev, 0xbbU, (uint8_t)0x7bU);
	pci_vdev_write_cfg_u8(vdev, 0xbcU, (uint8_t)0x1U);
	pci_vdev_write_cfg_u8(vdev, 0xbfU, (uint8_t)0x80U);
	pci_vdev_write_cfg_u8(vdev, 0xe0U, (uint8_t)0x9U);
	pci_vdev_write_cfg_u8(vdev, 0xe2U, (uint8_t)0xcU);
	pci_vdev_write_cfg_u8(vdev, 0xe3U, (uint8_t)0x1U);
	pci_vdev_write_cfg_u8(vdev, 0xf5U, (uint8_t)0xfU);
	pci_vdev_write_cfg_u8(vdev, 0xf6U, (uint8_t)0x1cU);
	pci_vdev_write_cfg_u8(vdev, 0xf7U, (uint8_t)0x1U);
}

void vdev_hostbridge_deinit(__unused const struct pci_vdev *vdev)
{
}

int32_t vdev_hostbridge_cfgread(const struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t *val)
{
	*val = pci_vdev_read_cfg(vdev, offset, bytes);

	return 0;
}

int32_t vdev_hostbridge_cfgwrite(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t val)
{
	if (!pci_bar_access(offset)) {
		pci_vdev_write_cfg(vdev, offset, bytes, val);
	}

	return 0;
}
