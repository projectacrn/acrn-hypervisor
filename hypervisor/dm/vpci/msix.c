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

static inline bool msixcap_access(struct pci_vdev *vdev, uint32_t offset)
{
	if (vdev->msix.capoff == 0U) {
		return 0;
	}

	return in_range(offset, vdev->msix.capoff, vdev->msix.caplen);
}

static inline bool msixtable_access(struct pci_vdev *vdev, uint32_t offset)
{
	return in_range(offset, vdev->msix.table_offset, vdev->msix.table_count * MSIX_TABLE_ENTRY_SIZE);
}

static void vmsix_disable_entry(struct pci_vdev *vdev, uint32_t index)
{
	struct msix_table_entry *pentry;
	uint64_t hva;

	hva = vdev->msix.mmio_hva + vdev->msix.table_offset;
	pentry = (struct msix_table_entry *)hva + index;
	mmio_write32(vdev->msix.tables[index].vector_control, (const void *)&(pentry->vector_control));
}

static int vmsix_update_entry(struct pci_vdev *vdev, uint32_t index)
{
	struct msix_table_entry *pentry;
	struct ptdev_msi_info info;
	uint64_t hva;
	int ret;

	info.is_msix = 1;
	info.vmsi_addr = vdev->msix.tables[index].addr;
	info.vmsi_data = vdev->msix.tables[index].data;

	ret = ptdev_msix_remap(vdev->vpci->vm, vdev->vbdf.value, (uint16_t)index, &info);
	if (ret != 0) {
		return ret;
	}

	/* Write the table entry to the physical structure */
	hva = vdev->msix.mmio_hva + vdev->msix.table_offset;
	pentry = (struct msix_table_entry *)hva + index;

	/*
	 * PCI 3.0 Spec allows writing to Message Address and Message Upper Address
	 * fields with a single QWORD write, but some hardware can accept 32 bits
	 * write only
	 */
	mmio_write32((uint32_t)(info.pmsi_addr), (const void *)&(pentry->addr));
	mmio_write32((uint32_t)(info.pmsi_addr >> 32U), (const void *)((char *)&(pentry->addr) + 4U));

	mmio_write32(info.pmsi_data, (const void *)&(pentry->data));
	mmio_write32(vdev->msix.tables[index].vector_control, (const void *)&(pentry->vector_control));

	return ret;
}

/* Do MSI-X remap for all MSI-X table entries in the target device */
static int vmsix_enable(struct pci_vdev *vdev)
{
	uint32_t msgoff = vdev->msix.capoff + PCIR_MSIX_CTRL;
	uint32_t msgctrl, index;
	int ret;

	for (index = 0U; index < vdev->msix.table_count; index++) {
		if ((vdev->msix.tables[index].vector_control & PCIM_MSIX_VCTRL_MASK) == 0U) {
			ret = vmsix_update_entry(vdev, index);
			if (ret != 0) {
				return ret;
			}
		}
	}

	/* make sure INTxDIS bit is set */
	enable_disable_pci_intx(vdev->pdev.bdf, false);

	msgctrl = pci_vdev_read_cfg(vdev, msgoff, 2U);
	pci_pdev_write_cfg(vdev->pdev.bdf, msgoff, 2U, msgctrl);

	return 0;
}

static void vmsix_disable(struct pci_vdev *vdev)
{
	uint32_t msgoff = vdev->msix.capoff + PCIR_MSIX_CTRL;
	uint32_t msgctrl;

	/* MSI-X Enable bit is cleared in vdev */
	msgctrl = pci_vdev_read_cfg(vdev, msgoff, 2U);
	pci_pdev_write_cfg(vdev->pdev.bdf, msgoff, 2U, msgctrl);
}

static int vmsix_cfgread(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	/* For PIO access, we emulate Capability Structures only */
	if (msixcap_access(vdev, offset)) {
		*val = pci_vdev_read_cfg(vdev, offset, bytes);
		return 0;
	}

	return -ENODEV;
}

static int vmsix_cfgwrite(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	uint32_t msgctrl;

	/* Writing MSI-X Capability Structure */
	if (msixcap_access(vdev, offset)) {
		msgctrl = pci_vdev_read_cfg(vdev, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U);

		/* Write to vdev */
		pci_vdev_write_cfg(vdev, offset, bytes, val);

		/* Writing Message Control field? */
		if ((offset - vdev->msix.capoff) == PCIR_MSIX_CTRL) {
			if (((msgctrl ^ val) & PCIM_MSIXCTRL_MSIX_ENABLE) != 0U) {
				if ((val & PCIM_MSIXCTRL_MSIX_ENABLE) != 0U) {
					(void)vmsix_enable(vdev);
				} else {
					vmsix_disable(vdev);
				}
			}

			if (((msgctrl ^ val) & PCIM_MSIXCTRL_FUNCTION_MASK) != 0U) {
				pci_pdev_write_cfg(vdev->pdev.bdf, offset, 2U, val);
			}
		}

		return 0;
	}

	return -ENODEV;
}

static void vmsix_table_rw(struct pci_vdev *vdev, struct mmio_request *mmio, uint32_t offset)
{
	struct msix_table_entry *entry;
	uint32_t vector_control, entry_offset, index;
	bool message_changed = false;
	bool unmasked;

	/* Find out which entry it's accessing */
	offset -= vdev->msix.table_offset;
	index = offset / MSIX_TABLE_ENTRY_SIZE;
	if (index >= vdev->msix.table_count) {
		pr_err("%s, invalid arguments %llx - %llx", __func__, mmio->value, mmio->address);
		return;
	}

	entry = &vdev->msix.tables[index];
	entry_offset = offset % MSIX_TABLE_ENTRY_SIZE;

	if (mmio->direction == REQUEST_READ) {
		(void)memcpy_s(&mmio->value, (size_t)mmio->size, (void *)entry + entry_offset, (size_t)mmio->size);
	} else {
		/* Only DWORD and QWORD are permitted */
		if ((mmio->size != 4U) && (mmio->size != 8U)) {
			pr_err("%s, Only DWORD and QWORD are permitted", __func__);
			return;
		}

		/* Save for comparison */
		vector_control = entry->vector_control;

		/*
		 * Writing different value to Message Data/Addr?
		 * PCI Spec: Software is permitted to fill in MSI-X Table entry DWORD fields individually
		 * with DWORD writes, or software in certain cases is permitted to fill in appropriate pairs
		 * of DWORDs with a single QWORD write
		 */
		if (entry_offset < offsetof(struct msix_table_entry, data)) {
			uint64_t qword_mask = ~0U;
			if (mmio->size == 4U) {
				qword_mask = (entry_offset == 0U) ? 0x00000000FFFFFFFFU : 0xFFFFFFFF00000000U;
			}
			message_changed = ((entry->addr & qword_mask) != (mmio->value & qword_mask));
		} else {
			if (entry_offset == offsetof(struct msix_table_entry, data)) {
				message_changed = (entry->data != (uint32_t)mmio->value);
			}
		}

		/* Write to pci_vdev */
		(void)memcpy_s((void *)entry + entry_offset, (size_t)mmio->size, &mmio->value, (size_t)mmio->size);

		/* If MSI-X hasn't been enabled, do nothing */
		if ((pci_vdev_read_cfg(vdev, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U) & PCIM_MSIXCTRL_MSIX_ENABLE)
			== PCIM_MSIXCTRL_MSIX_ENABLE) {

			unmasked = ((entry->vector_control & PCIM_MSIX_VCTRL_MASK) == 0U);
			if (((entry->vector_control ^ vector_control) & PCIM_MSIX_VCTRL_MASK) != 0U) {
				if (unmasked) {
					(void)vmsix_update_entry(vdev, index);
				} else {
					vmsix_disable_entry(vdev, index);
				}
			} else {
				if (message_changed && unmasked) {
					(void)vmsix_update_entry(vdev, index);
				}
			}
		}
	}
}

static int vmsix_table_mmio_access_handler(struct io_request *io_req, void *handler_private_data)
{
	struct mmio_request *mmio = &io_req->reqs.mmio;
	struct pci_vdev *vdev;
	uint64_t offset;
	uint64_t hva;

	vdev = (struct pci_vdev *)handler_private_data;
	offset = mmio->address - vdev->msix.intercepted_gpa;

	if (msixtable_access(vdev, (uint32_t)offset)) {
		vmsix_table_rw(vdev, mmio, (uint32_t)offset);
	} else {
		hva = vdev->msix.mmio_hva + offset;

		/* Only DWORD and QWORD are permitted */
		if ((mmio->size != 4U) && (mmio->size != 8U)) {
			pr_err("%s, Only DWORD and QWORD are permitted", __func__);
			return -EINVAL;
		}

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
				mmio_write32((uint32_t)(mmio->value), (const void *)hva);
			} else {
				mmio_write64(mmio->value, (const void *)hva);
			}
		}
	}

	return 0;
}

static void decode_msix_table_bar(struct pci_vdev *vdev)
{
	uint32_t bir = vdev->msix.table_bar;
	union pci_bdf pbdf = vdev->pdev.bdf;
	uint64_t base, size;
	uint32_t bar_lo, bar_hi, val32;

	bar_lo = pci_pdev_read_cfg(pbdf, pci_bar_offset(bir), 4U);
	if ((bar_lo & PCIM_BAR_SPACE) == PCIM_BAR_IO_SPACE) {
		/* I/O bar, should never happen */
		pr_err("PCI device (%x) has MSI-X Table at IO BAR", vdev->vbdf.value);
		return;
	}

	/* Get the base address */
	base = (uint64_t)bar_lo & PCIM_BAR_MEM_BASE;
	if ((bar_lo & PCIM_BAR_MEM_TYPE) == PCIM_BAR_MEM_64) {
		bar_hi = pci_pdev_read_cfg(pbdf, pci_bar_offset(bir + 1U), 4U);
		base |= ((uint64_t)bar_hi << 32U);
	}

	vdev->msix.mmio_hva = (uint64_t)hpa2hva(base);
	vdev->msix.mmio_gpa = vm0_hpa2gpa(base);

	/* Sizing the BAR */
	size = 0U;
	if (((bar_lo & PCIM_BAR_MEM_TYPE) == PCIM_BAR_MEM_64) && (bir < (PCI_BAR_COUNT - 1U))) {
		pci_pdev_write_cfg(pbdf, pci_bar_offset(bir + 1U), 4U, ~0U);
		size = (uint64_t)pci_pdev_read_cfg(pbdf, pci_bar_offset(bir + 1U), 4U);
		size <<= 32U;
	}

	pci_pdev_write_cfg(pbdf, pci_bar_offset(bir), 4U, ~0U);
	val32 = pci_pdev_read_cfg(pbdf, pci_bar_offset(bir), 4U);
	size |= ((uint64_t)val32 & PCIM_BAR_MEM_BASE);

	vdev->msix.mmio_size = size & ~(size - 1U);

	/* Restore the BAR */
	pci_pdev_write_cfg(pbdf, pci_bar_offset(bir), 4U, bar_lo);

	if ((bar_lo & PCIM_BAR_MEM_TYPE) == PCIM_BAR_MEM_64) {
		pci_pdev_write_cfg(pbdf, pci_bar_offset(bir + 1U), 4U, bar_hi);
	}
}

static int vmsix_init(struct pci_vdev *vdev)
{
	uint32_t msgctrl;
	uint32_t table_info, i;
	uint64_t addr_hi, addr_lo;
	struct msix *msix = &vdev->msix;

	msgctrl = pci_pdev_read_cfg(vdev->pdev.bdf, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U);

	/* Read Table Offset and Table BIR */
	table_info = pci_pdev_read_cfg(vdev->pdev.bdf, msix->capoff + PCIR_MSIX_TABLE, 4U);

	msix->table_bar = table_info & PCIM_MSIX_BIR_MASK;
	msix->table_offset = table_info & ~PCIM_MSIX_BIR_MASK;
	msix->table_count = (msgctrl & PCIM_MSIXCTRL_TABLE_SIZE) + 1U;

	if (msix->table_bar >= (PCI_BAR_COUNT - 1U)) {
		pr_err("%s, MSI-X device (%x) invalid table BIR %d", __func__, vdev->pdev.bdf.value, msix->table_bar);
		vdev->msix.capoff = 0U;
		return -EIO;
	}

	/* Mask all table entries */
	for (i = 0U; i < msix->table_count; i++) {
		msix->tables[i].vector_control = PCIM_MSIX_VCTRL_MASK;
		msix->tables[i].addr = 0U;
		msix->tables[i].data = 0U;
	}

	decode_msix_table_bar(vdev);

	if (msix->mmio_gpa != 0U) {
		/*
		 * PCI Spec: a BAR may also map other usable address space that is not associated
		 * with MSI-X structures, but it must not share any naturally aligned 4 KB
		 * address range with one where either MSI-X structure resides.
		 * The MSI-X Table and MSI-X PBA are permitted to co-reside within a naturally
		 * aligned 4 KB address range.
		 *
		 * If PBA or others reside in the same BAR with MSI-X Table, devicemodel could
		 * emulate them and maps these memory range at the 4KB boundary. Here, we should
		 * make sure only intercept the minimum number of 4K pages needed for MSI-X table.
		 */

		/* The higher boundary of the 4KB aligned address range for MSI-X table */
		addr_hi = msix->mmio_gpa + msix->table_offset + msix->table_count * MSIX_TABLE_ENTRY_SIZE;
		addr_hi = round_page_up(addr_hi);

		/* The lower boundary of the 4KB aligned address range for MSI-X table */
		addr_lo = round_page_down(msix->mmio_gpa + msix->table_offset);

		msix->intercepted_gpa = addr_lo;
		msix->intercepted_size = addr_hi - addr_lo;

		(void)register_mmio_emulation_handler(vdev->vpci->vm, vmsix_table_mmio_access_handler,
			msix->intercepted_gpa, msix->intercepted_gpa + msix->intercepted_size, vdev);
	}

	return 0;
}

static int vmsix_deinit(struct pci_vdev *vdev)
{
	vdev->msix.intercepted_size = 0U;

	if (vdev->msix.table_count != 0U) {
		ptdev_remove_msix_remapping(vdev->vpci->vm, vdev->vbdf.value, vdev->msix.table_count);
	}

	return 0;
}

struct pci_vdev_ops pci_ops_vdev_msix = {
	.init = vmsix_init,
	.deinit = vmsix_deinit,
	.cfgwrite = vmsix_cfgwrite,
	.cfgread = vmsix_cfgread,
};
