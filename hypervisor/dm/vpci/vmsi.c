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
#include <assign.h>
#include <vpci.h>
#include "vpci_priv.h"


/**
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
static inline void enable_disable_msi(const struct pci_vdev *vdev, bool enable)
{
	union pci_bdf pbdf = vdev->pdev->bdf;
	uint32_t capoff = vdev->msi.capoff;
	uint32_t msgctrl = pci_pdev_read_cfg(pbdf, capoff + PCIR_MSI_CTRL, 2U);

	if (enable) {
		msgctrl |= PCIM_MSICTRL_MSI_ENABLE;
	} else {
		msgctrl &= ~PCIM_MSICTRL_MSI_ENABLE;
	}
	pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_CTRL, 2U, msgctrl);
}
/**
 * @brief Remap vMSI virtual address and data to MSI physical address and data
 * This function is called when physical MSI is disabled.
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->pdev != NULL
 */
static void remap_vmsi(const struct pci_vdev *vdev)
{
	struct msi_info info = {};
	union pci_bdf pbdf = vdev->pdev->bdf;
	struct acrn_vm *vm = vpci2vm(vdev->vpci);
	uint32_t capoff = vdev->msi.capoff;
	uint32_t vmsi_msgdata, vmsi_addrlo, vmsi_addrhi = 0U;

	/* Read the MSI capability structure from virtual device */
	vmsi_addrlo = pci_vdev_read_vcfg(vdev, (capoff + PCIR_MSI_ADDR), 4U);
	if (vdev->msi.is_64bit) {
		vmsi_addrhi = pci_vdev_read_vcfg(vdev, (capoff + PCIR_MSI_ADDR_HIGH), 4U);
		vmsi_msgdata = pci_vdev_read_vcfg(vdev, (capoff + PCIR_MSI_DATA_64BIT), 2U);
	} else {
		vmsi_msgdata = pci_vdev_read_vcfg(vdev, (capoff + PCIR_MSI_DATA), 2U);
	}
	info.addr.full = (uint64_t)vmsi_addrlo | ((uint64_t)vmsi_addrhi << 32U);
	info.data.full = vmsi_msgdata;

	if (ptirq_prepare_msix_remap(vm, vdev->bdf.value, pbdf.value, 0U, &info) == 0) {
		pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_ADDR, 0x4U, (uint32_t)info.addr.full);
		if (vdev->msi.is_64bit) {
			pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_ADDR_HIGH, 0x4U,
					(uint32_t)(info.addr.full >> 32U));
			pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_DATA_64BIT, 0x2U, (uint16_t)info.data.full);
		} else {
			pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_DATA, 0x2U, (uint16_t)info.data.full);
		}

		/* If MSI Enable is being set, make sure INTxDIS bit is set */
		enable_disable_pci_intx(pbdf, false);
		enable_disable_msi(vdev, true);
	}
}

/**
 * @pre vdev != NULL
 */
void read_vmsi_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	/* For PIO access, we emulate Capability Structures only */
	*val = pci_vdev_read_vcfg(vdev, offset, bytes);
}

/**
 * @brief Writing MSI Capability Structure
 *
 * @pre vdev != NULL
 */
void write_vmsi_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	uint32_t msgctrl;

	enable_disable_msi(vdev, false);
	pci_vdev_write_vcfg(vdev, offset, bytes, val);

	msgctrl = pci_vdev_read_vcfg(vdev, vdev->msi.capoff + PCIR_MSI_CTRL, 2U);
	if ((msgctrl & PCIM_MSICTRL_MSI_ENABLE) != 0U) {
		remap_vmsi(vdev);
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
void deinit_vmsi(struct pci_vdev *vdev)
{
	if (has_msi_cap(vdev)) {
		ptirq_remove_msix_remapping(vpci2vm(vdev->vpci), vdev->pdev->bdf.value, 1U);
		(void)memset((void *)&vdev->msi, 0U, sizeof(struct pci_msi));
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void init_vmsi(struct pci_vdev *vdev)
{
	struct pci_pdev *pdev = vdev->pdev;
	uint32_t val;

	vdev->msi.capoff = pdev->msi_capoff;

	if (has_msi_cap(vdev)) {
		val = pci_pdev_read_cfg(pdev->bdf, vdev->msi.capoff, 4U);
		vdev->msi.caplen = ((val & (PCIM_MSICTRL_64BIT << 16U)) != 0U) ? 14U : 10U;
		vdev->msi.is_64bit = ((val & (PCIM_MSICTRL_64BIT << 16U)) != 0U);

		val &= ~((uint32_t)PCIM_MSICTRL_MMC_MASK << 16U);
		val &= ~((uint32_t)PCIM_MSICTRL_MME_MASK << 16U);
		pci_vdev_write_vcfg(vdev, vdev->msi.capoff, 4U, val);
	}
}

