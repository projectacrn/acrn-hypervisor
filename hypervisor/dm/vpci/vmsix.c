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
#include <vtd.h>
#include "vpci_priv.h"

/**
 * @pre vdev != NULL
 */
static inline bool msixtable_access(const struct pci_vdev *vdev, uint32_t offset)
{
	return in_range(offset, vdev->msix.table_offset, vdev->msix.table_count * MSIX_TABLE_ENTRY_SIZE);
}

/**
 * @pre vdev != NULL
 */
static inline struct msix_table_entry *get_msix_table_entry(const struct pci_vdev *vdev, uint32_t index)
{
	void *hva = hpa2hva(vdev->msix.mmio_hpa + vdev->msix.table_offset);
	return ((struct msix_table_entry *)hva + index);
}

/**
 * @pre vdev != NULL
 */
static void mask_one_msix_vector(const struct pci_vdev *vdev, uint32_t index)
{
	uint32_t vector_control;
	struct msix_table_entry *pentry = get_msix_table_entry(vdev, index);

	stac();
	vector_control = pentry->vector_control | PCIM_MSIX_VCTRL_MASK;
	mmio_write32(vector_control, (void *)&(pentry->vector_control));
	clac();
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->pdev != NULL
 */
static void remap_one_vmsix_entry(const struct pci_vdev *vdev, uint32_t index)
{
	const struct msix_table_entry *ventry;
	struct msix_table_entry *pentry;
	struct msi_info info = {};
	int32_t ret;

	mask_one_msix_vector(vdev, index);
	ventry = &vdev->msix.table_entries[index];
	if ((ventry->vector_control & PCIM_MSIX_VCTRL_MASK) == 0U) {
		info.addr.full = vdev->msix.table_entries[index].addr;
		info.data.full = vdev->msix.table_entries[index].data;

		ret = ptirq_prepare_msix_remap(vpci2vm(vdev->vpci), vdev->bdf.value, vdev->pdev->bdf.value,
					       (uint16_t)index, &info, INVALID_IRTE_ID);
		if (ret == 0) {
			/* Write the table entry to the physical structure */
			pentry = get_msix_table_entry(vdev, index);

			/*
			 * PCI 3.0 Spec allows writing to Message Address and Message Upper Address
			 * fields with a single QWORD write, but some hardware can accept 32 bits
			 * write only
			 */
			stac();
			mmio_write32((uint32_t)(info.addr.full), (void *)&(pentry->addr));
			mmio_write32((uint32_t)(info.addr.full >> 32U), (void *)((char *)&(pentry->addr) + 4U));

			mmio_write32(info.data.full, (void *)&(pentry->data));
			mmio_write32(vdev->msix.table_entries[index].vector_control, (void *)&(pentry->vector_control));
			clac();
		}
	}

}

/**
 * @pre vdev != NULL
 */
void read_vmsix_cap_reg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	/* For PIO access, we emulate Capability Structures only */
	*val = pci_vdev_read_vcfg(vdev, offset, bytes);
}

/**
 * @brief Writing MSI-X Capability Structure
 *
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void write_vmsix_cap_reg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	static const uint8_t msix_ro_mask[12U] = {
		0xffU, 0xffU, 0xffU, 0x3fU,	/* Only Function Mask and MSI-X Enable writable */
		0xffU, 0xffU, 0xffU, 0xffU,
		0xffU, 0xffU, 0xffU, 0xffU };
	uint32_t msgctrl, old, ro_mask = ~0U;

	(void)memcpy_s((void *)&ro_mask, bytes, (void *)&msix_ro_mask[offset - vdev->msix.capoff], bytes);
	if (ro_mask != ~0U) {
		old = pci_vdev_read_vcfg(vdev, vdev->msix.capoff, bytes);
		pci_vdev_write_vcfg(vdev, offset, bytes, (old & ro_mask) | (val & ~ro_mask));

		msgctrl = pci_vdev_read_vcfg(vdev, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U);
		/* If MSI Enable is being set, make sure INTxDIS bit is set */
		if ((msgctrl & PCIM_MSIXCTRL_MSIX_ENABLE) != 0U) {
			enable_disable_pci_intx(vdev->pdev->bdf, false);
		}
		pci_pdev_write_cfg(vdev->pdev->bdf, offset, 2U, msgctrl);
	}
}

/**
 * @pre vdev != NULL
 * @pre mmio != NULL
 */
static void rw_vmsix_table(struct pci_vdev *vdev, struct mmio_request *mmio, uint32_t offset)
{
	struct msix_table_entry *entry;
	uint32_t entry_offset, table_offset, index;

	/* Find out which entry it's accessing */
	table_offset = offset - vdev->msix.table_offset;
	index = table_offset / MSIX_TABLE_ENTRY_SIZE;

	if (index < vdev->msix.table_count) {
		entry = &vdev->msix.table_entries[index];
		entry_offset = table_offset % MSIX_TABLE_ENTRY_SIZE;

		if (mmio->direction == REQUEST_READ) {
			(void)memcpy_s(&mmio->value, (size_t)mmio->size,
					(void *)entry + entry_offset, (size_t)mmio->size);
		} else {
			/* Only DWORD and QWORD are permitted */
			if ((mmio->size == 4U) || (mmio->size == 8U)) {
				/* Write to pci_vdev */
				(void)memcpy_s((void *)entry + entry_offset, (size_t)mmio->size,
						&mmio->value, (size_t)mmio->size);
				remap_one_vmsix_entry(vdev, index);
			} else {
				pr_err("%s, Only DWORD and QWORD are permitted", __func__);
			}

		}
	} else {
		pr_err("%s, invalid arguments %lx - %lx", __func__, mmio->value, mmio->address);
	}

}

/**
 * @pre io_req != NULL
 * @pre handler_private_data != NULL
 */
int32_t vmsix_handle_table_mmio_access(struct io_request *io_req, void *handler_private_data)
{
	struct mmio_request *mmio = &io_req->reqs.mmio;
	struct pci_vdev *vdev;
	int32_t ret = 0;
	uint64_t offset;
	void *hva;

	vdev = (struct pci_vdev *)handler_private_data;
	/* This device has not be assigned to other OS */
	if (vdev->user == vdev) {
		offset = mmio->address - vdev->msix.mmio_gpa;

		if (msixtable_access(vdev, (uint32_t)offset)) {
			rw_vmsix_table(vdev, mmio, (uint32_t)offset);
		} else {
			hva = hpa2hva(vdev->msix.mmio_hpa + offset);

			/* Only DWORD and QWORD are permitted */
			if ((mmio->size == 4U) || (mmio->size == 8U)) {
				if (hva != NULL) {
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
				}
			} else {
				pr_err("%s, Only DWORD and QWORD are permitted", __func__);
				ret = -EINVAL;
			}
		}
	} else {
		ret = -EFAULT;
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
 */
void deinit_vmsix(struct pci_vdev *vdev)
{
	if (has_msix_cap(vdev)) {
		if (vdev->msix.table_count != 0U) {
			ptirq_remove_msix_remapping(vpci2vm(vdev->vpci), vdev->pdev->bdf.value, vdev->msix.table_count);
			(void)memset((void *)&vdev->msix, 0U, sizeof(struct pci_msix));
		}
	}
}
