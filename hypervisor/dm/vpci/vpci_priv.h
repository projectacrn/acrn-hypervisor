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

#ifndef VPCI_PRIV_H_
#define VPCI_PRIV_H_

#include <pci.h>

static inline bool in_range(uint32_t value, uint32_t lower, uint32_t len)
{
	return ((value >= lower) && (value < (lower + len)));
}

static inline uint8_t pci_vdev_read_cfg_u8(const struct pci_vdev *vdev, uint32_t offset)
{
	return vdev->cfgdata.data_8[offset];
}

static inline uint16_t pci_vdev_read_cfg_u16(const struct pci_vdev *vdev, uint32_t offset)
{
	return vdev->cfgdata.data_16[offset >> 1U];
}

static inline uint32_t pci_vdev_read_cfg_u32(const struct pci_vdev *vdev, uint32_t offset)
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

/**
 * @pre vdev != NULL
 */
static inline bool has_msix_cap(const struct pci_vdev *vdev)
{
	return (vdev->msix.capoff != 0U);
}

/**
 * @pre vdev != NULL
 */
static inline bool is_hostbridge(const struct pci_vdev *vdev)
{
	return (vdev->vbdf.value == 0U);
}

void init_vhostbridge(struct pci_vdev *vdev);
int32_t vhostbridge_cfgread(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val);
int32_t vhostbridge_cfgwrite(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);
void deinit_vhostbridge(__unused const struct pci_vdev *vdev);

void init_vdev_pt(struct pci_vdev *vdev);
int32_t vdev_pt_cfgread(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val);
int32_t vdev_pt_cfgwrite(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);

void init_vmsi(struct pci_vdev *vdev);
int32_t vmsi_cfgread(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val);
int32_t vmsi_cfgwrite(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);
void deinit_vmsi(const struct pci_vdev *vdev);

void init_vmsix(struct pci_vdev *vdev);
void vdev_pt_remap_msix_table_bar(struct pci_vdev *vdev);
int32_t vmsix_table_mmio_access_handler(struct io_request *io_req, void *handler_private_data);
int32_t vmsix_cfgread(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val);
int32_t vmsix_cfgwrite(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);
void deinit_vmsix(const struct pci_vdev *vdev);

uint32_t pci_vdev_read_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes);
void pci_vdev_write_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);

struct pci_vdev *pci_find_vdev_by_vbdf(const struct acrn_vpci *vpci, union pci_bdf vbdf);

struct pci_vdev *pci_find_vdev_by_pbdf(const struct acrn_vpci *vpci, union pci_bdf pbdf);

void partition_mode_cfgread(const struct acrn_vpci *vpci, union pci_bdf vbdf,
	uint32_t offset, uint32_t bytes, uint32_t *val);
void partition_mode_cfgwrite(const struct acrn_vpci *vpci, union pci_bdf vbdf,
	uint32_t offset, uint32_t bytes, uint32_t val);

void sharing_mode_cfgread(struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t *val);
void sharing_mode_cfgwrite(__unused struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t val);

#endif /* VPCI_PRIV_H_ */
