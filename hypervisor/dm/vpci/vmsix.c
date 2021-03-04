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

#include <x86/guest/vm.h>
#include <errno.h>
#include <vpci.h>
#include <x86/guest/ept.h>
#include <x86/mmu.h>
#include <logmsg.h>
#include "vpci_priv.h"

/**
 * @brief Writing MSI-X Capability Structure
 *
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
bool write_vmsix_cap_reg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	static const uint8_t msix_ro_mask[12U] = {
		0xffU, 0xffU, 0xffU, 0x3fU,	/* Only Function Mask and MSI-X Enable writable */
		0xffU, 0xffU, 0xffU, 0xffU,
		0xffU, 0xffU, 0xffU, 0xffU };
	bool is_written = false;
	uint32_t old, ro_mask = ~0U;

	(void)memcpy_s((void *)&ro_mask, bytes, (void *)&msix_ro_mask[offset - vdev->msix.capoff], bytes);
	if (ro_mask != ~0U) {
		old = pci_vdev_read_vcfg(vdev, offset, bytes);
		pci_vdev_write_vcfg(vdev, offset, bytes, (old & ro_mask) | (val & ~ro_mask));
		is_written = true;
	}

	return is_written;
}

/**
 * @pre vdev != NULL
 * @pre io_req != NULL
 * @pre mmio->address >= vdev->msix.mmio_gpa
 */
uint32_t rw_vmsix_table(struct pci_vdev *vdev, struct io_request *io_req)
{
	struct mmio_request *mmio = &io_req->reqs.mmio;
	struct msix_table_entry *entry;
	uint32_t entry_offset, table_offset, index = CONFIG_MAX_MSIX_TABLE_NUM;
	uint64_t offset;

	/* Must be full DWORD or full QWORD aligned. */
	if ((mmio->size == 4U) || (mmio->size == 8U)) {
		offset = mmio->address - vdev->msix.mmio_gpa;
		if (msixtable_access(vdev, (uint32_t)offset)) {

			table_offset = (uint32_t)(offset - vdev->msix.table_offset);
			index = table_offset / MSIX_TABLE_ENTRY_SIZE;

			entry = &vdev->msix.table_entries[index];
			entry_offset = table_offset % MSIX_TABLE_ENTRY_SIZE;

			if (mmio->direction == REQUEST_READ) {
				(void)memcpy_s(&mmio->value, (size_t)mmio->size,
					(void *)entry + entry_offset, (size_t)mmio->size);
			} else {
				(void)memcpy_s((void *)entry + entry_offset, (size_t)mmio->size,
					&mmio->value, (size_t)mmio->size);
			}
		} else if (mmio->direction == REQUEST_READ) {
			mmio->value = 0UL;
		}
	} else {
		pr_err("%s, Only DWORD and QWORD are permitted", __func__);
	}

	return index;
}

/**
 * @pre io_req != NULL
 * @pre priv_data != NULL
 */
int32_t vmsix_handle_table_mmio_access(struct io_request *io_req, void *priv_data)
{
	(void)rw_vmsix_table((struct pci_vdev *)priv_data, io_req);
	return 0;
}

/**
 * @pre vdev != NULL
 */
int32_t add_vmsix_capability(struct pci_vdev *vdev, uint32_t entry_num, uint8_t bar_num)
{
	uint32_t table_size, i;
	struct msixcap msixcap;
	int32_t ret = -1;

	if ((bar_num < PCI_BAR_COUNT) &&
		(entry_num <= min(CONFIG_MAX_MSIX_TABLE_NUM, VMSIX_MAX_TABLE_ENTRY_NUM))) {

		table_size = VMSIX_MAX_ENTRY_TABLE_SIZE;

		vdev->msix.caplen = MSIX_CAPLEN;
		vdev->msix.table_bar = bar_num;
		vdev->msix.table_offset = 0U;
		vdev->msix.table_count = entry_num;

		/* set mask bit of vector control register */
		for (i = 0; i < entry_num; i++)
			vdev->msix.table_entries[i].vector_control |= PCIM_MSIX_VCTRL_MASK;

		(void)memset(&msixcap, 0U, sizeof(struct msixcap));

		msixcap.capid = PCIY_MSIX;
		msixcap.msgctrl = entry_num - 1U;

		/* - MSI-X table start at offset 0 */
		msixcap.table_info = bar_num;
		msixcap.pba_info = table_size | bar_num;

		vdev->msix.capoff = vpci_add_capability(vdev, (uint8_t *)(&msixcap), sizeof(struct msixcap));
		if (vdev->msix.capoff != 0U) {
			ret = 0;
		}
	}
	return ret;
}
