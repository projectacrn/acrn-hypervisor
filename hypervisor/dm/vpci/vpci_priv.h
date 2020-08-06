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

#include <list.h>
#include <pci.h>

static inline struct acrn_vm *vpci2vm(const struct acrn_vpci *vpci)
{
	return container_of(vpci, struct acrn_vm, vpci);
}

static inline bool is_quirk_ptdev(const struct pci_vdev *vdev)
{
	return ((vdev->flags & QUIRK_PTDEV) != 0U);
}

static inline bool in_range(uint32_t value, uint32_t lower, uint32_t len)
{
	return ((value >= lower) && (value < (lower + len)));
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
static inline bool msixcap_access(const struct pci_vdev *vdev, uint32_t offset)
{
	return (has_msix_cap(vdev) && in_range(offset, vdev->msix.capoff, vdev->msix.caplen));
}

/*
 * @pre vdev != NULL
 */
static inline bool has_sriov_cap(const struct pci_vdev *vdev)
{
	return (vdev->sriov.capoff != 0U);
}

/*
 * @pre vdev != NULL
 */
static inline bool sriovcap_access(const struct pci_vdev *vdev, uint32_t offset)
{
	return (has_sriov_cap(vdev) && in_range(offset, vdev->sriov.capoff, vdev->sriov.caplen));
}

/**
 * @pre vdev != NULL
 */
static inline bool vbar_access(const struct pci_vdev *vdev, uint32_t offset)
{
	return is_bar_offset(vdev->nr_bars, offset);
}

/**
 * @pre vdev != NULL
 */
static inline bool cfg_header_access(uint32_t offset)
{
	return (offset < PCI_CFG_HEADER_LENGTH);
}

/**
 * @pre vdev != NULL
 */
static inline bool has_msi_cap(const struct pci_vdev *vdev)
{
	return (vdev->msi.capoff != 0U);
}

/**
 * @pre vdev != NULL
 */
static inline bool msicap_access(const struct pci_vdev *vdev, uint32_t offset)
{
	return (has_msi_cap(vdev) && in_range(offset, vdev->msi.capoff, vdev->msi.caplen));
}

/**
 * @brief Check if the specified vdev is a zombie VF instance
 *
 * @pre: The vdev is a VF instance
 *
 * @param vdev Pointer to vdev instance
 *
 * @return If the vdev is a zombie VF instance return true, otherwise return false
 */
static inline bool is_zombie_vf(const struct pci_vdev *vdev)
{
	return (vdev->user == NULL);
}

void init_vdev_pt(struct pci_vdev *vdev, bool is_pf_vdev);
void deinit_vdev_pt(struct pci_vdev *vdev);
void vdev_pt_write_vbar(struct pci_vdev *vdev, uint32_t idx, uint32_t val);
void vdev_pt_map_msix(struct pci_vdev *vdev, bool hold_lock);

void init_vmsi(struct pci_vdev *vdev);
void read_vmsi_cap_reg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val);
void write_vmsi_cap_reg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);
void deinit_vmsi(const struct pci_vdev *vdev);

void init_vmsix(struct pci_vdev *vdev);
int32_t vmsix_handle_table_mmio_access(struct io_request *io_req, void *handler_private_data);
void read_vmsix_cap_reg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val);
void write_vmsix_cap_reg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);
void deinit_vmsix(struct pci_vdev *vdev);

void init_vmsix_on_msi(struct pci_vdev *vdev);
void write_vmsix_cap_reg_on_msi(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);
void remap_one_vmsix_entry_on_msi(struct pci_vdev *vdev, uint32_t index);

void init_vsriov(struct pci_vdev *vdev);
void read_sriov_cap_reg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val);
void write_sriov_cap_reg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);
uint32_t sriov_bar_offset(const struct pci_vdev *vdev, uint32_t bar_idx);

uint32_t pci_vdev_read_vcfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes);
void pci_vdev_write_vcfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);

uint32_t pci_vdev_read_vbar(const struct pci_vdev *vdev, uint32_t idx);
void pci_vdev_write_vbar(struct pci_vdev *vdev, uint32_t idx, uint32_t val);

void vdev_pt_hide_sriov_cap(struct pci_vdev *vdev);

typedef void (*map_pcibar)(struct pci_vdev *vdev, uint32_t bar_idx);
typedef void (*unmap_pcibar)(struct pci_vdev *vdev, uint32_t bar_idx);
void vpci_update_one_vbar(struct pci_vdev *vdev, uint32_t bar_idx, uint32_t val, map_pcibar map_cb, unmap_pcibar unmap_cb);
#endif /* VPCI_PRIV_H_ */
