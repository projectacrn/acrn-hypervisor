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

#include <hypervisor.h>
#include "pci_priv.h"

static inline bool msicap_access(struct pci_vdev *vdev, uint32_t offset)
{
	if (vdev->msi.capoff == 0U) {
		return 0;
	}

	return in_range(offset, vdev->msi.capoff, vdev->msi.caplen);
}

static int vmsi_remap(struct pci_vdev *vdev, bool enable)
{
	struct ptdev_msi_info info;
	union pci_bdf pbdf = vdev->pdev.bdf;
	struct acrn_vm *vm = vdev->vpci->vm;
	uint32_t capoff = vdev->msi.capoff;
	uint32_t msgctrl, msgdata;
	uint32_t addrlo, addrhi;
	int ret;

	/* Disable MSI during configuration */
	msgctrl = pci_vdev_read_cfg(vdev, capoff + PCIR_MSI_CTRL, 2U);
	if ((msgctrl & PCIM_MSICTRL_MSI_ENABLE) == PCIM_MSICTRL_MSI_ENABLE) {
		pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_CTRL, 2U, msgctrl & ~PCIM_MSICTRL_MSI_ENABLE);
	}

	/* Read the MSI capability structure from virtual device */
	addrlo = pci_vdev_read_cfg_u32(vdev, capoff + PCIR_MSI_ADDR);
	if (msgctrl & PCIM_MSICTRL_64BIT) {
		msgdata = pci_vdev_read_cfg_u16(vdev, capoff + PCIR_MSI_DATA_64BIT);
		addrhi = pci_vdev_read_cfg_u32(vdev, capoff + PCIR_MSI_ADDR_HIGH);
	} else {
		msgdata = pci_vdev_read_cfg_u16(vdev, capoff + PCIR_MSI_DATA);
		addrhi = 0U;
	}

	info.is_msix = 0;
	info.vmsi_addr = (uint64_t)addrlo | ((uint64_t)addrhi << 32U);

	/* MSI is being enabled or disabled */
	if (enable) {
		info.vmsi_data = msgdata;
	} else {
		info.vmsi_data = 0U;
	}

	ret = ptdev_msix_remap(vm, vdev->vbdf.value, 0U, &info);
	if (ret != 0) {
		return ret;
	}

	/* Update MSI Capability structure to physical device */
	pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_ADDR, 0x4U, (uint32_t)info.pmsi_addr);
	if (msgctrl & PCIM_MSICTRL_64BIT) {
		pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_ADDR_HIGH, 0x4U,	(uint32_t)(info.pmsi_addr >> 32U));
		pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_DATA_64BIT, 0x2U, (uint16_t)info.pmsi_data);
	} else {
		pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_DATA, 0x2U, (uint16_t)info.pmsi_data);
	}

	/* If MSI Enable is being set, make sure INTxDIS bit is set */
	if (enable) {
		enable_disable_pci_intx(pbdf, false);
		pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_CTRL, 2U, msgctrl | PCIM_MSICTRL_MSI_ENABLE);
	}

	return ret;
}

static int vmsi_cfgread(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	/* For PIO access, we emulate Capability Structures only */
	if (msicap_access(vdev, offset)) {
		*val = pci_vdev_read_cfg(vdev, offset, bytes);
		return 0;
	}

	return -ENODEV;
}

static int vmsi_cfgwrite(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	bool message_changed = false;
	bool enable;
	uint32_t msgctrl;

	/* Writing MSI Capability Structure */
	if (msicap_access(vdev, offset)) {

		/* Save msgctrl for comparison */
		msgctrl = pci_vdev_read_cfg(vdev, vdev->msi.capoff + PCIR_MSI_CTRL, 2U);

		/* Either Message Data or message Addr is being changed */
		if (((offset - vdev->msi.capoff) >= PCIR_MSI_ADDR) && (val != pci_vdev_read_cfg(vdev, offset, bytes))) {
			message_changed = true;
		}

		/* Write to vdev */
		pci_vdev_write_cfg(vdev, offset, bytes, val);

		/* Do remap if MSI Enable bit is being changed */
		if (((offset - vdev->msi.capoff) == PCIR_MSI_CTRL) && ((msgctrl ^ val) & PCIM_MSICTRL_MSI_ENABLE)) {
			enable = ((val & PCIM_MSICTRL_MSI_ENABLE) != 0U);
			(void)vmsi_remap(vdev, enable);
		} else {
			if (message_changed && ((msgctrl & PCIM_MSICTRL_MSI_ENABLE) != 0U)) {
				(void)vmsi_remap(vdev, true);
			}
		}

		return 0;
	}

	return -ENODEV;
}

void populate_msi_struct(struct pci_vdev *vdev)
{
	uint8_t ptr, cap;
	uint32_t msgctrl;
	uint32_t len, bytes, offset, val;
	union pci_bdf pbdf = vdev->pdev.bdf;

	/* Has new Capabilities list? */
	if ((pci_pdev_read_cfg(pbdf, PCIR_STATUS, 2U) & PCIM_STATUS_CAPPRESENT) == 0U) {
		return;
	}

	ptr = (uint8_t)pci_pdev_read_cfg(pbdf, PCIR_CAP_PTR, 1U);
	while ((ptr != 0U) && (ptr != 0xFFU)) {
		cap = (uint8_t)pci_pdev_read_cfg(pbdf, ptr + PCICAP_ID, 1U);

		/* Ignore all other Capability IDs for now */
		if ((cap == PCIY_MSI) || (cap == PCIY_MSIX)) {
			offset = ptr;
			if (cap == PCIY_MSI) {
				vdev->msi.capoff = offset;
				msgctrl = pci_pdev_read_cfg(pbdf, offset + PCIR_MSI_CTRL, 2U);

				/*
				 * Ignore the 'mask' and 'pending' bits in the MSI capability
				 * (msgctrl & PCIM_MSICTRL_VECTOR).
				 * We'll let the guest manipulate them directly.
				 */
				len = (msgctrl & PCIM_MSICTRL_64BIT) ? 14U : 10U;
				vdev->msi.caplen = len;

				/* Assign MSI handler for configuration read and write */
				add_vdev_handler(vdev, &pci_ops_vdev_msi);
			} else {
				vdev->msix.capoff = offset;
				vdev->msix.caplen = MSIX_CAPLEN;
				len = vdev->msix.caplen;

				/* Assign MSI-X handler for configuration read and write */
				add_vdev_handler(vdev, &pci_ops_vdev_msix);
			}

			/* Copy MSI/MSI-X capability struct into virtual device */
			while (len > 0U) {
				bytes = (len >= 4U) ? 4U : len;
				val = pci_pdev_read_cfg(pbdf, offset, bytes);

				if ((cap == PCIY_MSI) && (offset == vdev->msi.capoff)) {
					/*
					 * Don't support multiple vector for now,
					 * Force Multiple Message Enable and Multiple Message
					 * Capable to 0
					 */
					val &= ~((uint32_t)PCIM_MSICTRL_MMC_MASK << 16U);
					val &= ~((uint32_t)PCIM_MSICTRL_MME_MASK << 16U);
				}

				pci_vdev_write_cfg(vdev, offset, bytes, val);
				len -= bytes;
				offset += bytes;
			}
		}

		ptr = (uint8_t)pci_pdev_read_cfg(pbdf, ptr + PCICAP_NEXTPTR, 1U);
	}
}

static int vmsi_deinit(struct pci_vdev *vdev)
{
	if (vdev->msi.capoff != 0U) {
		ptdev_remove_msix_remapping(vdev->vpci->vm, vdev->vbdf.value, 1);
	}

	return 0;
}

struct pci_vdev_ops pci_ops_vdev_msi = {
	.init = NULL,
	.deinit = vmsi_deinit,
	.cfgwrite = vmsi_cfgwrite,
	.cfgread = vmsi_cfgread,
};
