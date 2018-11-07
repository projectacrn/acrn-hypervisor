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

#include <hypervisor.h>
#include "pci_priv.h"

static inline uint32_t pci_bar_base(uint32_t bar)
{
	return bar & PCIM_BAR_MEM_BASE;
}

static int vdev_pt_init_validate(struct pci_vdev *vdev)
{
	uint32_t idx;

	for (idx = 0U; idx < PCI_BAR_COUNT; idx++) {
		if ((vdev->bar[idx].base != 0x0UL)
			|| ((vdev->bar[idx].size & 0xFFFUL) != 0x0UL)
			|| ((vdev->bar[idx].type != PCIBAR_MEM32)
			&& (vdev->bar[idx].type != PCIBAR_NONE))) {
			return -EINVAL;
		}
	}

	return 0;
}

static int vdev_pt_init(struct pci_vdev *vdev)
{
	int ret;
	struct acrn_vm *vm = vdev->vpci->vm;
	uint16_t pci_command;

	ret = vdev_pt_init_validate(vdev);
	if (ret != 0) {
		pr_err("Error, invalid bar defined");
		return ret;
	}

	/* Create an iommu domain for target VM if not created */
	if (vm->iommu == NULL) {
		if (vm->arch_vm.nworld_eptp == 0UL) {
			vm->arch_vm.nworld_eptp = vm->arch_vm.ept_mem_ops.get_pml4_page(vm->arch_vm.ept_mem_ops.info, 0UL);
			sanitize_pte((uint64_t *)vm->arch_vm.nworld_eptp);
		}
		vm->iommu = create_iommu_domain(vm->vm_id,
			hva2hpa(vm->arch_vm.nworld_eptp), 48U);
	}

	ret = assign_iommu_device(vm->iommu, (uint8_t)vdev->pdev.bdf.bits.b,
		(uint8_t)(vdev->pdev.bdf.value & 0xFFU));

	pci_command = (uint16_t)pci_pdev_read_cfg(vdev->pdev.bdf, PCIR_COMMAND, 2U);
	/* Disable INTX */
	pci_command |= 0x400U;
	pci_pdev_write_cfg(vdev->pdev.bdf, PCIR_COMMAND, 2U, pci_command);

	return ret;
}

static int vdev_pt_deinit(struct pci_vdev *vdev)
{
	int ret;
	struct acrn_vm *vm = vdev->vpci->vm;

	ret = unassign_iommu_device(vm->iommu, (uint8_t)vdev->pdev.bdf.bits.b,
		(uint8_t)(vdev->pdev.bdf.value & 0xFFU));

	return ret;
}

static int vdev_pt_cfgread(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t *val)
{
	/* Assumption: access needed to be aligned on 1/2/4 bytes */
	if ((offset & (bytes - 1U)) != 0U) {
		*val = 0xFFFFFFFFU;
		return -EINVAL;
	}

	/* PCI BARs is emulated */
	if (pci_bar_access(offset)) {
		*val = pci_vdev_read_cfg(vdev, offset, bytes);
	} else {
		*val = pci_pdev_read_cfg(vdev->pdev.bdf, offset, bytes);
	}

	return 0;
}

static void vdev_pt_remap_bar(struct pci_vdev *vdev, uint32_t idx,
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
			vdev->pdev.bar[idx].base, /* HPA */
			new_base, /*GPA*/
			vdev->bar[idx].size,
			EPT_WR | EPT_RD | EPT_UNCACHED);
	}
}

static void vdev_pt_cfgwrite_bar(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t new_bar_uos)
{
	uint32_t idx;
	uint32_t new_bar, mask;
	bool bar_update_normal;

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
		new_bar = new_bar_uos & mask;
		if (bar_update_normal) {
			vdev_pt_remap_bar(vdev, idx,
				pci_bar_base(new_bar));

			vdev->bar[idx].base = pci_bar_base(new_bar);
		}
		break;

	default:
		pr_err("Unknown bar type, idx=%d", idx);
		break;
	}

	pci_vdev_write_cfg_u32(vdev, offset, new_bar);
}

static int vdev_pt_cfgwrite(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t val)
{
	/* Assumption: access needed to be aligned on 1/2/4 bytes */
	if ((offset & (bytes - 1U)) != 0U) {
		return -EINVAL;
	}

	/* PCI BARs are emulated */
	if (pci_bar_access(offset)) {
		vdev_pt_cfgwrite_bar(vdev, offset, bytes, val);
	} else {
		/* Write directly to physical device's config space */
		pci_pdev_write_cfg(vdev->pdev.bdf, offset, bytes, val);
	}

	return 0;
}

struct pci_vdev_ops pci_ops_vdev_pt = {
	.init = vdev_pt_init,
	.deinit = vdev_pt_deinit,
	.cfgread = vdev_pt_cfgread,
	.cfgwrite = vdev_pt_cfgwrite,
};

