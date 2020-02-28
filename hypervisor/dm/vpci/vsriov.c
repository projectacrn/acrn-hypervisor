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
#include <ptdev.h>
#include <vpci.h>
#include <logmsg.h>

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
static void create_vf(struct pci_vdev *pf_vdev, union pci_bdf vf_bdf)
{
	/* Implementation in next patch */
	(void)pf_vdev;
	(void)vf_bdf;
}

/**
 * @pre pf_vdev != NULL
 * @pre is_vf_enabled(pf_dev) == true
 * @Application constraints: PCIR_SRIOV_NUMVFS register value cannot be 0 if VF_ENABLE is set.
 */
static void enable_vf(struct pci_vdev *pf_vdev)
{
	union pci_bdf vf_bdf;
	uint16_t idx;
	uint16_t sub_vid = 0U;

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
	sub_vid = (uint16_t) pci_pdev_read_cfg(vf_bdf, PCIV_SUB_VENDOR_ID, 2U);
	if ((sub_vid != 0xFFFFU) && (sub_vid != 0U)) {
		uint16_t num_vfs, stride, fst_off;

		num_vfs = read_sriov_reg(pf_vdev, PCIR_SRIOV_NUMVFS);
		fst_off = read_sriov_reg(pf_vdev, PCIR_SRIOV_FST_VF_OFF);
		stride = read_sriov_reg(pf_vdev, PCIR_SRIOV_VF_STRIDE);
		for (idx = 0U; idx < num_vfs; idx++) {
			vf_bdf.fields.bus = get_vf_bus(pf_vdev, fst_off, stride, idx);
			vf_bdf.fields.devfun = get_vf_devfun(pf_vdev, fst_off, stride, idx);

			/* if one VF has never been created then create new pdev/vdev for this VF */
			if (pci_find_vdev(&pf_vdev->vpci->vm->vpci, vf_bdf) == NULL) {
				create_vf(pf_vdev, vf_bdf);
			}
		}
	} else {
		/*
		 * If the VF physical device was not created successfully, the pdev/vdev
		 * will also not be created so that SOS can aware of VF creation failure,
		 */
		pr_err("PF %x:%x.%x can't create VFs after 100 ms",
			pf_vdev->bdf.bits.b, pf_vdev->bdf.bits.d, pf_vdev->bdf.bits.f);
	}
}

/**
 * @pre pf_vdev != NULL
 */
static void disable_vf(struct pci_vdev *pf_vdev)
{
	/* Implementation in next patch */
	(void)pf_vdev;
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
	/* no need to do emulation, passthrough to physical device directly */
	*val = pci_pdev_read_cfg(vdev->pdev->bdf, offset, bytes);
}

/**
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void write_sriov_cap_reg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{

	uint32_t reg;

	reg = offset - vdev->sriov.capoff;
	if (reg == PCIR_SRIOV_CONTROL) {
		bool enable;

		enable = (((val & PCIM_SRIOV_VF_ENABLE) != 0U) ? true : false);
		if (enable != is_vf_enabled(vdev)) {
			if (enable) {
				/*
				 * set VF_ENABLE to PF physical device before enable_vf
				 * since need to ask hardware to create VF physical
				 * devices firstly
				 */
				pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, val);
				enable_vf(vdev);
			} else {
				disable_vf(vdev);
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


/**
 * @pre vdev != NULL
 */
uint32_t sriov_bar_offset(const struct pci_vdev *vdev, uint32_t bar_idx)
{
	return (vdev->sriov.capoff + PCIR_SRIOV_VF_BAR_OFF + (bar_idx << 2U));
}
