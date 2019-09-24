/*
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

#include <vm.h>
#include "vpci_priv.h"

/**
 * @pre vdev != NULL
 */
uint32_t pci_vdev_read_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes)
{
	uint32_t val;

	switch (bytes) {
	case 1U:
		val = pci_vdev_read_cfg_u8(vdev, offset);
		break;
	case 2U:
		val = pci_vdev_read_cfg_u16(vdev, offset);
		break;
	default:
		val = pci_vdev_read_cfg_u32(vdev, offset);
		break;
	}

	return val;
}

/**
 * @pre vdev != NULL
 */
void pci_vdev_write_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	switch (bytes) {
	case 1U:
		pci_vdev_write_cfg_u8(vdev, offset, (uint8_t)val);
		break;
	case 2U:
		pci_vdev_write_cfg_u16(vdev, offset, (uint16_t)val);
		break;
	default:
		pci_vdev_write_cfg_u32(vdev, offset, val);
		break;
	}
}

/**
 * @pre vpci != NULL
 * @pre vpci->pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 */
struct pci_vdev *pci_find_vdev(const struct acrn_vpci *vpci, union pci_bdf vbdf)
{
	struct pci_vdev *vdev, *tmp;
	uint32_t i;

	vdev = NULL;
	for (i = 0U; i < vpci->pci_vdev_cnt; i++) {
		tmp = (struct pci_vdev *)&(vpci->pci_vdevs[i]);

		if (bdf_is_equal(tmp->bdf, vbdf)) {
			vdev = tmp;
			break;
		}
	}

	return vdev;
}
