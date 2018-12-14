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

#ifndef PCI_PRIV_H_
#define PCI_PRIV_H_

#include <pci.h>

static inline bool in_range(uint32_t value, uint32_t lower, uint32_t len)
{
	return ((value >= lower) && (value < (lower + len)));
}

static inline uint8_t pci_vdev_read_cfg_u8(struct pci_vdev *vdev, uint32_t offset)
{
	return vdev->cfgdata.data_8[offset];
}

static inline uint16_t pci_vdev_read_cfg_u16(struct pci_vdev *vdev, uint32_t offset)
{
	return vdev->cfgdata.data_16[offset >> 1U];
}

static inline uint32_t pci_vdev_read_cfg_u32(struct pci_vdev *vdev, uint32_t offset)
{
	return vdev->cfgdata.data_32[offset >> 2U];
}

static inline void pci_vdev_write_cfg_u8(struct pci_vdev *vdev, uint32_t offset, uint8_t val)
{
	vdev->cfgdata.data_8[offset] = val;
}

static inline void pci_vdev_write_cfg_u16(struct pci_vdev *vdev, uint32_t offset, uint16_t val)
{
	vdev->cfgdata.data_16[offset >> 1U] = val;
}

static inline void pci_vdev_write_cfg_u32(struct pci_vdev *vdev, uint32_t offset, uint32_t val)
{
	vdev->cfgdata.data_32[offset >> 2U] = val;
}

#ifdef CONFIG_PARTITION_MODE
extern const struct vpci_ops partition_mode_vpci_ops;
#else
extern const struct vpci_ops sharing_mode_vpci_ops;
extern const struct pci_vdev_ops pci_ops_vdev_msix;
#endif

uint32_t pci_vdev_read_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes);
void pci_vdev_write_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);

void populate_msi_struct(struct pci_vdev *vdev);

struct pci_vdev *sharing_mode_find_vdev(union pci_bdf pbdf);
void add_vdev_handler(struct pci_vdev *vdev, const struct pci_vdev_ops *ops);

#endif /* PCI_PRIV_H_ */
