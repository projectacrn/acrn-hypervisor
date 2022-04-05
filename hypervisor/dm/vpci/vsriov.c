/*
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2018 Intel Corporation
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

#include <asm/guest/vm.h>
#include <ptdev.h>
#include <vpci.h>
#include <asm/pci_dev.h>
#include <logmsg.h>
#include <delay.h>

#include "vpci_priv.h"

/**
 * @pre pf_vdev != NULL
 */
static inline uint8_t get_vf_devfun(const struct pci_vdev *pf_vdev, uint16_t fst_off, uint16_t stride, uint16_t id)
{
	return ((uint8_t)((pf_vdev->bdf.fields.devfun + fst_off + (stride * id)) & 0xFFU));
}

/**
 * @pre pf_vdev != NULL
 */
static inline uint8_t get_vf_bus(const struct pci_vdev *pf_vdev, uint16_t fst_off, uint16_t stride, uint16_t id)
{
	return ((uint8_t)(pf_vdev->bdf.fields.bus + ((pf_vdev->bdf.fields.devfun + fst_off + (stride * id)) >> 8U)));
}

/**
 * @pre pf_vdev != NULL
 */
static inline uint16_t read_sriov_reg(const struct pci_vdev *pf_vdev, uint16_t reg)
{
	return ((uint16_t)(pci_pdev_read_cfg(pf_vdev->bdf, pf_vdev->sriov.capoff + reg, 2U)));
}

/**
 * @pre pf_vdev != NULL
 */
static bool is_vf_enabled(const struct pci_vdev *pf_vdev)
{
	uint16_t control;

	control = read_sriov_reg(pf_vdev, PCIR_SRIOV_CONTROL);
	return ((control & PCIM_SRIOV_VF_ENABLE) != 0U);
}

/**
 * @pre pf_vdev != NULL
 */
static void init_sriov_vf_bar(struct pci_vdev *pf_vdev)
{
	init_vdev_pt(pf_vdev, true);
}

/**
 * @pre pf_vdev != NULL
 */
static void create_vf(struct pci_vdev *pf_vdev, union pci_bdf vf_bdf, uint16_t vf_id)
{
	struct pci_pdev *vf_pdev;
	struct pci_vdev *vf_vdev = NULL;

	/*
	 * Per VT-d 8.3.3, the VFs are under the scope of the same
	 * remapping unit as the associated PF when SRIOV is enabled.
	 */
	vf_pdev = pci_init_pdev(vf_bdf, pf_vdev->pdev->drhd_index);
	if (vf_pdev != NULL) {
		struct acrn_vm_pci_dev_config *dev_cfg;

		dev_cfg = init_one_dev_config(vf_pdev);
		if (dev_cfg != NULL) {
			vf_vdev = vpci_init_vdev(&vpci2vm(pf_vdev->vpci)->vpci, dev_cfg, pf_vdev);
		}
	}

	/*
	 * if a VF vdev failed to be created, the VF number is less than requested number
	 * and the requested VF physical devices are ready at this time, clear VF_ENABLE.
	 */
	if (vf_vdev == NULL) {
		uint16_t control;

		control = read_sriov_reg(pf_vdev, PCIR_SRIOV_CONTROL);
		control &= (~PCIM_SRIOV_VF_ENABLE);
		pci_pdev_write_cfg(pf_vdev->bdf, pf_vdev->sriov.capoff + PCIR_SRIOV_CONTROL, 2U, control);
		pr_err("PF %x:%x.%x can't creat VF, unset VF_ENABLE",
			pf_vdev->bdf.bits.b, pf_vdev->bdf.bits.d, pf_vdev->bdf.bits.f);
	} else {
		uint32_t bar_idx;
		struct pci_vbar *vf_vbar;

		/* VF bars information from its PF SRIOV capability, no need to access physical device */
		vf_vdev->nr_bars = PCI_BAR_COUNT;
		for (bar_idx = 0U; bar_idx < PCI_BAR_COUNT; bar_idx++) {
			vf_vbar = &vf_vdev->vbars[bar_idx];
			*vf_vbar = vf_vdev->phyfun->sriov.vbars[bar_idx];
			vf_vbar->base_hpa += (vf_vbar->size * vf_id);
			vf_vbar->base_gpa = vf_vbar->base_hpa;
			if (has_msix_cap(vf_vdev) && (bar_idx == vf_vdev->msix.table_bar)) {
				vf_vdev->msix.mmio_hpa = vf_vbar->base_hpa;
				vf_vdev->msix.mmio_size = vf_vbar->size;
			}
			/*
			 * VF BARs value are zero and read only, according to PCI Express
			 * Base 4.0 chapter 9.3.4.1.11, the VF
			 */
			pci_vdev_write_vcfg(vf_vdev, pci_bar_offset(bar_idx), 4U, 0U);
		}

		if (has_msix_cap(vf_vdev)) {
			vdev_pt_map_msix(vf_vdev, false);
		}
	}
}

/**
 * @pre pf_vdev != NULL
 * @pre is_vf_enabled(pf_dev) == true
 * @Application constraints: PCIR_SRIOV_NUMVFS register value cannot be 0 if VF_ENABLE is set.
 */
static void enable_vfs(struct pci_vdev *pf_vdev)
{
	union pci_bdf vf_bdf;
	uint16_t idx;
	uint16_t sub_vid = 0U;
	uint16_t num_vfs, stride, fst_off;

	/* Confirm that the physical VF_ENABLE register has been set successfully */
	ASSERT(is_vf_enabled(pf_vdev), "VF_ENABLE was not set successfully on the hardware");

	/*
         * All VFs bars information are located at PF VF_BAR fields of SRIOV capability.
	 * Initialize the PF's VF_BAR registers before initialize each VF device bar.
	 */
	init_sriov_vf_bar(pf_vdev);

	/*
	 * Per PCIE base spec 9.3.3.3.1, VF Enable bit from cleared to set, the
	 * system is not perrmitted to issue requests to the VFs until one of
	 * the following is true:
	 * 1. at least 100ms has passed.
	 * 2. An FRS message has been received from the PF with a reason code
	 *    of VF Enabled.
	 * 3. At least VF Enable Time has passed since VF Enable was Set.
	 *    VF Enable Time is either the Reset Time value in the Readiness Time
	 *    Reporting capability associated with the VF or a value determined
	 *    by system software/firmware.
	 *
	 * Curerntly, we use the first way to wait for VF physical devices to be ready.
	 */
	udelay (100U * 1000U);

	/*
	 * Due to VF's DEVICE ID and VENDOR ID are 0xFFFF, so check if VF physical
	 * device has been created by the value of SUBSYSTEM VENDOR ID.
	 * To check if all enabled VFs are ready, just check the first VF already exists,
	 * do not need to check all.
	 */
	fst_off = read_sriov_reg(pf_vdev, PCIR_SRIOV_FST_VF_OFF);
	stride = read_sriov_reg(pf_vdev, PCIR_SRIOV_VF_STRIDE);
	vf_bdf.fields.bus = get_vf_bus(pf_vdev, fst_off, stride, 0U);
	vf_bdf.fields.devfun = get_vf_devfun(pf_vdev, fst_off, stride, 0U);
	sub_vid = (uint16_t) pci_pdev_read_cfg(vf_bdf, PCIV_SUB_VENDOR_ID, 2U);
	if ((sub_vid != 0xFFFFU) && (sub_vid != 0U)) {
		struct pci_vdev *vf_vdev;

		num_vfs = read_sriov_reg(pf_vdev, PCIR_SRIOV_NUMVFS);
		for (idx = 0U; idx < num_vfs; idx++) {
			vf_bdf.fields.bus = get_vf_bus(pf_vdev, fst_off, stride, idx);
			vf_bdf.fields.devfun = get_vf_devfun(pf_vdev, fst_off, stride, idx);

			/*
			 * If one VF has never been created then create new one including pdev/vdev structures.
			 *
			 * The VF maybe have already existed but it is a zombie instance that vf_vdev->vpci
			 * is NULL, in this case, we need to make the vf_vdev available again in here.
			 */
			vf_vdev = pci_find_vdev(&vpci2vm(pf_vdev->vpci)->vpci, vf_bdf);
			if (vf_vdev == NULL) {
				create_vf(pf_vdev, vf_bdf, idx);
			} else {
				/* Re-activate a zombie VF */
				if (is_zombie_vf(vf_vdev)) {
					vf_vdev->vdev_ops->init_vdev(vf_vdev);
				}
			}
		}
	} else {
		/*
		 * If the VF physical device was not created successfully, the pdev/vdev
		 * will also not be created so that Service VM can aware of VF creation failure,
		 */
		pr_err("PF %x:%x.%x can't create VFs after 100 ms",
			pf_vdev->bdf.bits.b, pf_vdev->bdf.bits.d, pf_vdev->bdf.bits.f);
	}
}

/**
 * @pre pf_vdev != NULL
 */
static void disable_vfs(struct pci_vdev *pf_vdev)
{
	uint16_t idx, num_vfs, stride, first;
	struct pci_vdev *vf_vdev;

	/*
	 * PF can disable VFs only when all VFs are not used by any VM or any application
	 *
	 * Ideally, VF instances should be deleted after VFs are disabled, but for FuSa reasons,
	 * we simply set the VF instance status to "zombie" to avoid dynamically adding/removing
	 * resources
	 *
	 * If the VF drivers are still running in Service VM or User VM, the MMIO access will return 0xFF.
	 */
	num_vfs = read_sriov_reg(pf_vdev, PCIR_SRIOV_NUMVFS);
	first = read_sriov_reg(pf_vdev, PCIR_SRIOV_FST_VF_OFF);
	stride = read_sriov_reg(pf_vdev, PCIR_SRIOV_VF_STRIDE);
	for (idx = 0U; idx < num_vfs; idx++) {
		union pci_bdf bdf;

		bdf.fields.bus = get_vf_bus(pf_vdev, first, stride, idx);
		bdf.fields.devfun = get_vf_devfun(pf_vdev, first, stride, idx);
		vf_vdev = pci_find_vdev(&vpci2vm(pf_vdev->vpci)->vpci, bdf);
		if ((vf_vdev != NULL) && (!is_zombie_vf(vf_vdev))) {
			/* set disabled VF as zombie vdev instance */
			vf_vdev->vdev_ops->deinit_vdev(vf_vdev);
		}
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void init_vsriov(struct pci_vdev *vdev)
{
	struct pci_pdev *pdev = vdev->pdev;

	vdev->sriov.capoff = pdev->sriov.capoff;
	vdev->sriov.caplen = pdev->sriov.caplen;
}

/**
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void read_sriov_cap_reg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	if (!vdev->pdev->sriov.hide_sriov) {
		/* no need to do emulation, passthrough to physical device directly */
		*val = pci_pdev_read_cfg(vdev->pdev->bdf, offset, bytes);
	} else {
		*val = 0xffffffffU;
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void write_sriov_cap_reg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{

	uint32_t reg;

	reg = offset - vdev->sriov.capoff;
	if (!vdev->pdev->sriov.hide_sriov) {
		if (reg == PCIR_SRIOV_CONTROL) {
			bool enable;

			enable = ((val & PCIM_SRIOV_VF_ENABLE) != 0U);
			if (enable != is_vf_enabled(vdev)) {
				if (enable) {
					/*
					 * set VF_ENABLE to PF physical device before enable_vfs
					 * since need to ask hardware to create VF physical
					 * devices firstly
					 */
					pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, val);
					enable_vfs(vdev);
				} else {
					disable_vfs(vdev);
					pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, val);
				}
			} else {
				pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, val);
			}
		} else if (reg == PCIR_SRIOV_NUMVFS) {
			uint16_t total;

			total = read_sriov_reg(vdev, PCIR_SRIOV_TOTAL_VFS);
			/*
			 * sanity check for NumVFs register based on PCE Express Base 4.0 9.3.3.7 chapter
			 * The results are undefined if NumVFs is set to a value greater than TotalVFs
			 * NumVFs may only be written while VF Enable is Clear
			 * If NumVFs is written when VF Enable is Set, the results are undefined
			 */
			if ((((uint16_t)(val & 0xFFU)) <= total) && (!is_vf_enabled(vdev))) {
				pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, val);
			}
		} else {
			pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, val);
		}
	}
}


/**
 * @pre vdev != NULL
 */
uint32_t sriov_bar_offset(const struct pci_vdev *vdev, uint32_t bar_idx)
{
	return (vdev->sriov.capoff + PCIR_SRIOV_VF_BAR_OFF + (bar_idx << 2U));
}
