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

/* Passthrough PCI device related operations */

#include <vm.h>
#include <errno.h>
#include <ept.h>
#include <mmu.h>
#include <logmsg.h>
#include "vpci_priv.h"

static inline uint32_t pci_bar_base(uint32_t bar)
{
	return bar & PCIM_BAR_MEM_BASE;
}

/**
 * @pre vdev != NULL
 */
int32_t vdev_pt_cfgread(const struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t *val)
{
	int32_t ret = -ENODEV;

	/* PCI BARs is emulated */
	if (pci_bar_access(offset)) {
		*val = pci_vdev_read_cfg(vdev, offset, bytes);
		ret = 0;
	}

	return ret;
}

/**
* @pre vdev != NULL
* @pre vdev->pdev != NULL
* @pre vdev->pdev->msix.table_bar < (PCI_BAR_COUNT - 1U)
*/
void vdev_pt_remap_msix_table_bar(struct pci_vdev *vdev)
{
	uint32_t i;
	uint64_t addr_hi, addr_lo;
	struct pci_msix *msix = &vdev->msix;
	struct pci_pdev *pdev = vdev->pdev;
	struct pci_bar *bar;
	struct acrn_vm *vm = vdev->vpci->vm;
	struct acrn_vm_config *vm_config;

	vm_config = get_vm_config(vm->vm_id);

	ASSERT(vdev->pdev->msix.table_bar < (PCI_BAR_COUNT - 1U), "msix->table_bar out of range");


	/* Mask all table entries */
	for (i = 0U; i < msix->table_count; i++) {
		msix->tables[i].vector_control = PCIM_MSIX_VCTRL_MASK;
		msix->tables[i].addr = 0U;
		msix->tables[i].data = 0U;
	}

	bar = &pdev->bar[msix->table_bar];
	if (bar != NULL) {
		vdev->msix.mmio_hpa = bar->base;
		if (vm_config->load_order == PRE_LAUNCHED_VM) {
			vdev->msix.mmio_gpa = vdev->bar[msix->table_bar].base;
		} else {
			vdev->msix.mmio_gpa = sos_vm_hpa2gpa(bar->base);
		}
		vdev->msix.mmio_size = bar->size;
	}


	/*
	 *    For SOS:
	 *    --------
	 *    MSI-X Table BAR Contains:
	 *    Other Info + Tables + PBA	        Ohter info already mapped into EPT (since SOS)
	 *    					Tables are handled by HV MMIO handler (4k adjusted up and down)
	 *    						and remaps interrupts
	 *    					PBA already mapped into EPT (since SOS)
	 *
	 *    Other Info + Tables		Other info already mapped into EPT (since SOS)
	 *					Tables are handled by HV MMIO handler (4k adjusted up and down)
	 *						and remaps interrupts
	 *
	 *    Tables				Tables are handled by HV MMIO handler (4k adjusted up and down)
	 *    						and remaps interrupts
	 *
	 *    For UOS (launched by DM):
	 *    -------------------------
	 *    MSI-X Table BAR Contains:
	 *    Other Info + Tables + PBA		Other info  mapped into EPT (4k adjusted) by DM
	 *    					Tables are handled by DM MMIO handler (4k adjusted up and down) and SOS writes to tables,
	 *    						intercepted by HV MMIO handler and HV remaps interrupts
	 *    					PBA already mapped into EPT by DM
	 *
	 *    Other Info + Tables		Other info mapped into EPT by DM
	 *    					Tables are handled by DM MMIO handler (4k adjusted up and down) and SOS writes to tables,
	 *    						intercepted by HV MMIO handler and HV remaps interrupts.
	 *
	 *    Tables				Tables are handled by DM MMIO handler (4k adjusted up and down) and SOS writes to tables,
	 *    						intercepted by HV MMIO handler and HV remaps interrupts.
	 *
	 *    For Pre-launched VMs (no SOS/DM):
	 *    --------------------------------
	 *    MSI-X Table BAR Contains:
	 *    All 3 cases:			Writes to MMIO region in MSI-X Table BAR handled by HV MMIO handler
	 *    					If the offset falls within the MSI-X table [offset, offset+tables_size), HV remaps
	 *    						interrupts.
	 *    					Else, HV writes/reads to/from the corresponding HPA
	 */


	if (msix->mmio_gpa != 0U) {
		if (vm_config->load_order == PRE_LAUNCHED_VM) {
			addr_hi = vdev->msix.mmio_gpa + vdev->msix.mmio_size;
			addr_lo = vdev->msix.mmio_gpa;
		} else {
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
			addr_hi = msix->mmio_gpa + msix->table_offset + (msix->table_count * MSIX_TABLE_ENTRY_SIZE);
			addr_hi = round_page_up(addr_hi);

			/* The lower boundary of the 4KB aligned address range for MSI-X table */
			addr_lo = round_page_down(msix->mmio_gpa + msix->table_offset);
		}

		register_mmio_emulation_handler(vdev->vpci->vm, vmsix_table_mmio_access_handler,
			addr_lo, addr_hi, vdev);
	}
}

/**
 * @brief Remaps guest BARs other than MSI-x Table BAR
 * This API is invoked upon guest re-programming PCI BAR with MMIO region
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 */
static void vdev_pt_remap_generic_bar(const struct pci_vdev *vdev, uint32_t idx,
	uint32_t new_base)
{
	struct acrn_vm *vm = vdev->vpci->vm;

	if (vdev->bar[idx].base != 0UL) {
		ept_mr_del(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
			vdev->bar[idx].base,
			vdev->bar[idx].size);
	}

	if (new_base != 0U) {
		/* Map the physical BAR in the guest MMIO space */
		ept_mr_add(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
			vdev->pdev->bar[idx].base, /* HPA */
			new_base, /*GPA*/
			vdev->bar[idx].size,
			EPT_WR | EPT_RD | EPT_UNCACHED);
	}
}

/**
 * @pre vdev != NULL
 */
static void vdev_pt_cfgwrite_bar(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t new_bar_uos)
{
	uint32_t idx;
	uint32_t new_bar, mask;
	bool bar_update_normal;
	bool is_msix_table_bar;

	if ((bytes != 4U) || ((offset & 0x3U) != 0U)) {
		return;
	}

	new_bar = 0U;
	idx = (offset - pci_bar_offset(0U)) >> 2U;
	mask = ~(vdev->bar[idx].size - 1U);

	switch (vdev->bar[idx].type) {
	case PCIBAR_NONE:
		vdev->bar[idx].base = 0UL;
		break;

	case PCIBAR_MEM32:
		bar_update_normal = (new_bar_uos != (uint32_t)~0U);
		is_msix_table_bar = (has_msix_cap(vdev) && (idx == vdev->msix.table_bar));
		new_bar = new_bar_uos & mask;
		if (bar_update_normal) {
			if (is_msix_table_bar) {
				vdev->bar[idx].base = pci_bar_base(new_bar);
				vdev_pt_remap_msix_table_bar(vdev);
			} else {
				vdev_pt_remap_generic_bar(vdev, idx,
					pci_bar_base(new_bar));

				vdev->bar[idx].base = pci_bar_base(new_bar);
			}
		}
		break;

	default:
		pr_err("Unknown bar type, idx=%d", idx);
		break;
	}

	pci_vdev_write_cfg_u32(vdev, offset, new_bar);
}

/**
 * @pre vdev != NULL
 */
int32_t vdev_pt_cfgwrite(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t val)
{
	int32_t ret = -ENODEV;

	/* PCI BARs are emulated */
	if (pci_bar_access(offset)) {
		vdev_pt_cfgwrite_bar(vdev, offset, bytes, val);
		ret = 0;
	}

	return ret;
}
