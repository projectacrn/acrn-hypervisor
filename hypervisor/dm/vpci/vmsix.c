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
#include <errno.h>
#include <ptdev.h>
#include <assign.h>
#include <vpci.h>
#include <io.h>
#include <ept.h>
#include <mmu.h>
#include <logmsg.h>
#include "vpci_priv.h"


/**
 * @pre vdev != NULL
 */
static inline bool msixcap_access(const struct pci_vdev *vdev, uint32_t offset)
{
	bool ret = false;

	if (has_msix_cap(vdev)) {
		ret = in_range(offset, vdev->msix.capoff, vdev->msix.caplen);
	}

	return ret;
}

/**
 * @pre vdev != NULL
 */
static inline bool msixtable_access(const struct pci_vdev *vdev, uint32_t offset)
{
	return in_range(offset, vdev->msix.table_offset, vdev->msix.table_count * MSIX_TABLE_ENTRY_SIZE);
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre vdev->pdev != NULL
 */
static int32_t vmsix_remap_entry(const struct pci_vdev *vdev, uint32_t index, bool enable)
{
	struct msix_table_entry *pentry;
	struct ptirq_msi_info info;
	void *hva;
	int32_t ret;

	info.is_msix = 1;
	info.vmsi_addr.full = vdev->msix.tables[index].addr;
	info.vmsi_data.full = (enable) ? vdev->msix.tables[index].data : 0U;

	ret = ptirq_msix_remap(vdev->vpci->vm, vdev->vbdf.value, vdev->pdev->bdf.value, (uint16_t)index, &info);
	if (ret == 0) {
		/* Write the table entry to the physical structure */
		hva = hpa2hva(vdev->msix.mmio_hpa + vdev->msix.table_offset);
		pentry = (struct msix_table_entry *)hva + index;

		/*
		 * PCI 3.0 Spec allows writing to Message Address and Message Upper Address
		 * fields with a single QWORD write, but some hardware can accept 32 bits
		 * write only
		 */
		stac();
		mmio_write32((uint32_t)(info.pmsi_addr.full), (void *)&(pentry->addr));
		mmio_write32((uint32_t)(info.pmsi_addr.full >> 32U), (void *)((char *)&(pentry->addr) + 4U));

		mmio_write32(info.pmsi_data.full, (void *)&(pentry->data));
		mmio_write32(vdev->msix.tables[index].vector_control, (void *)&(pentry->vector_control));
		clac();
	}

	return ret;
}

/**
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
static inline void enable_disable_msix(const struct pci_vdev *vdev, bool enable)
{
	uint32_t msgctrl;

	msgctrl = pci_vdev_read_cfg(vdev, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U);
	if (enable) {
		msgctrl |= PCIM_MSIXCTRL_MSIX_ENABLE;
	} else {
		msgctrl &= ~PCIM_MSIXCTRL_MSIX_ENABLE;
	}
	pci_pdev_write_cfg(vdev->pdev->bdf, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U, msgctrl);
}

/**
 * Do MSI-X remap for all MSI-X table entries in the target device
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
static int32_t vmsix_remap(const struct pci_vdev *vdev, bool enable)
{
	uint32_t index;
	int32_t ret = 0;

	/* disable MSI-X during configuration */
	enable_disable_msix(vdev, false);

	for (index = 0U; index < vdev->msix.table_count; index++) {
		ret = vmsix_remap_entry(vdev, index, enable);
		if (ret != 0) {
			break;
		}
	}

	/* If MSI Enable is being set, make sure INTxDIS bit is set */
	if (ret == 0) {
		if (enable) {
			enable_disable_pci_intx(vdev->pdev->bdf, false);
		}
		enable_disable_msix(vdev, enable);
	}

	return ret;
}

/**
 * Do MSI-X remap for one MSI-X table entry only
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
static int32_t vmsix_remap_one_entry(const struct pci_vdev *vdev, uint32_t index, bool enable)
{
	uint32_t msgctrl;
	int32_t ret;

	/* disable MSI-X during configuration */
	enable_disable_msix(vdev, false);

	ret = vmsix_remap_entry(vdev, index, enable);
	if (ret == 0) {
		/* If MSI Enable is being set, make sure INTxDIS bit is set */
		if (enable) {
			enable_disable_pci_intx(vdev->pdev->bdf, false);
		}

		/* Restore MSI-X Enable bit */
		msgctrl = pci_vdev_read_cfg(vdev, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U);
		if ((msgctrl & PCIM_MSIXCTRL_MSIX_ENABLE) == PCIM_MSIXCTRL_MSIX_ENABLE) {
			pci_pdev_write_cfg(vdev->pdev->bdf, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U, msgctrl);
		}
	}

	return ret;
}

/**
 * @pre vdev != NULL
 */
int32_t vmsix_read_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	int32_t ret = -ENODEV;
	/* For PIO access, we emulate Capability Structures only */

	if (msixcap_access(vdev, offset)) {
		*val = pci_vdev_read_cfg(vdev, offset, bytes);
	    ret = 0;
	}

	return ret;
}

/**
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
int32_t vmsix_write_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	uint32_t msgctrl;
	int32_t ret = -ENODEV;

	/* Writing MSI-X Capability Structure */
	if (msixcap_access(vdev, offset)) {
		msgctrl = pci_vdev_read_cfg(vdev, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U);

		/* Write to vdev */
		pci_vdev_write_cfg(vdev, offset, bytes, val);

		/* Writing Message Control field? */
		if ((offset - vdev->msix.capoff) == PCIR_MSIX_CTRL) {
			if (((msgctrl ^ val) & PCIM_MSIXCTRL_MSIX_ENABLE) != 0U) {
				if ((val & PCIM_MSIXCTRL_MSIX_ENABLE) != 0U) {
					(void)vmsix_remap(vdev, true);
				} else {
					(void)vmsix_remap(vdev, false);
				}
			}

			if (((msgctrl ^ val) & PCIM_MSIXCTRL_FUNCTION_MASK) != 0U) {
				pci_pdev_write_cfg(vdev->pdev->bdf, offset, 2U, val);
			}
		}
		ret = 0;
	}

	return ret;
}

/**
 * @pre vdev != NULL
 * @pre mmio != NULL
 */
static void vmsix_table_rw(const struct pci_vdev *vdev, struct mmio_request *mmio, uint32_t offset)
{
	const struct msix_table_entry *entry;
	uint32_t vector_control, entry_offset, table_offset, index;
	bool message_changed = false;
	bool unmasked;

	/* Find out which entry it's accessing */
	table_offset = offset - vdev->msix.table_offset;
	index = table_offset / MSIX_TABLE_ENTRY_SIZE;

	if (index < vdev->msix.table_count) {
		entry = &vdev->msix.tables[index];
		entry_offset = table_offset % MSIX_TABLE_ENTRY_SIZE;

		if (mmio->direction == REQUEST_READ) {
			(void)memcpy_s(&mmio->value, (size_t)mmio->size,
					(void *)entry + entry_offset, (size_t)mmio->size);
		} else {
			/* Only DWORD and QWORD are permitted */
			if ((mmio->size == 4U) || (mmio->size == 8U)) {
				/* Save for comparison */
				vector_control = entry->vector_control;

				/*
				 * Writing different value to Message Data/Addr?
				 * PCI Spec: Software is permitted to fill in MSI-X Table entry DWORD fields
				 * individually with DWORD writes, or software in certain cases is permitted
				 * to fill in appropriate pairs of DWORDs with a single QWORD write
				 */
				if (entry_offset < offsetof(struct msix_table_entry, data)) {
					uint64_t qword_mask = ~0UL;

					if (mmio->size == 4U) {
						qword_mask = (entry_offset == 0U) ?
								0x00000000FFFFFFFFUL : 0xFFFFFFFF00000000UL;
					}
					message_changed = ((entry->addr & qword_mask) != (mmio->value & qword_mask));
				} else {
					if (entry_offset == offsetof(struct msix_table_entry, data)) {
						message_changed = (entry->data != (uint32_t)mmio->value);
					}
				}

				/* Write to pci_vdev */
				(void)memcpy_s((void *)entry + entry_offset, (size_t)mmio->size,
						&mmio->value, (size_t)mmio->size);

				/* If MSI-X hasn't been enabled, do nothing */
				if ((pci_vdev_read_cfg(vdev, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U)
						& PCIM_MSIXCTRL_MSIX_ENABLE) == PCIM_MSIXCTRL_MSIX_ENABLE) {

					if ((((entry->vector_control ^ vector_control) & PCIM_MSIX_VCTRL_MASK) != 0U)
							|| message_changed) {
						unmasked = ((entry->vector_control & PCIM_MSIX_VCTRL_MASK) == 0U);
						(void)vmsix_remap_one_entry(vdev, index, unmasked);
					}
				}
			} else {
				pr_err("%s, Only DWORD and QWORD are permitted", __func__);
			}

		}
	} else {
		pr_err("%s, invalid arguments %llx - %llx", __func__, mmio->value, mmio->address);
	}

}

/**
 * @pre io_req != NULL
 * @pre handler_private_data != NULL
 */
int32_t vmsix_table_mmio_access_handler(struct io_request *io_req, void *handler_private_data)
{
	struct mmio_request *mmio = &io_req->reqs.mmio;
	struct pci_vdev *vdev;
	int32_t ret = 0;
	uint64_t offset;
	void *hva;

	vdev = (struct pci_vdev *)handler_private_data;
	offset = mmio->address - vdev->msix.mmio_gpa;

	if (msixtable_access(vdev, (uint32_t)offset)) {
		vmsix_table_rw(vdev, mmio, (uint32_t)offset);
	} else {
		hva = hpa2hva(vdev->msix.mmio_hpa + offset);

		/* Only DWORD and QWORD are permitted */
		if ((mmio->size != 4U) && (mmio->size != 8U)) {
			pr_err("%s, Only DWORD and QWORD are permitted", __func__);
			ret = -EINVAL;
		} else if (hva != NULL) {
			stac();
			/* MSI-X PBA and Capability Table could be in the same range */
			if (mmio->direction == REQUEST_READ) {
				/* mmio->size is either 4U or 8U */
				if (mmio->size == 4U) {
					mmio->value = (uint64_t)mmio_read32((const void *)hva);
				} else {
					mmio->value = mmio_read64((const void *)hva);
				}
			} else {
				/* mmio->size is either 4U or 8U */
				if (mmio->size == 4U) {
					mmio_write32((uint32_t)(mmio->value), (void *)hva);
				} else {
					mmio_write64(mmio->value, (void *)hva);
				}
			}
			clac();
		} else {
			/* No other state currently, do nothing */
		}
	}

	return ret;
}

/**
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void init_vmsix(struct pci_vdev *vdev)
{
	struct pci_pdev *pdev = vdev->pdev;

	vdev->msix.capoff = pdev->msix.capoff;
	vdev->msix.caplen = pdev->msix.caplen;
	vdev->msix.table_bar = pdev->msix.table_bar;
	vdev->msix.table_offset = pdev->msix.table_offset;
	vdev->msix.table_count = pdev->msix.table_count;

	if (has_msix_cap(vdev)) {
		(void)memcpy_s((void *)&vdev->cfgdata.data_8[pdev->msix.capoff], pdev->msix.caplen,
			(void *)&pdev->msix.cap[0U], pdev->msix.caplen);
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 */
void deinit_vmsix(const struct pci_vdev *vdev)
{
	if (has_msix_cap(vdev)) {
		if (vdev->msix.table_count != 0U) {
			ptirq_remove_msix_remapping(vdev->vpci->vm, vdev->vbdf.value, vdev->msix.table_count);
		}
	}
}
