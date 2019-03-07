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
#include <vtd.h>
#include <ept.h>
#include <mmu.h>
#include <logmsg.h>
#include "pci_priv.h"

static inline uint32_t pci_bar_base(uint32_t bar)
{
	return bar & PCIM_BAR_MEM_BASE;
}

static int32_t validate(const struct pci_vdev *vdev)
{
	uint32_t idx;
	int32_t ret = 0;

	for (idx = 0U; idx < PCI_BAR_COUNT; idx++) {
		if ((vdev->bar[idx].base != 0x0UL)
			|| ((vdev->bar[idx].size & 0xFFFUL) != 0x0UL)
			|| ((vdev->bar[idx].type != PCIBAR_MEM32)
			&& (vdev->bar[idx].type != PCIBAR_NONE))) {
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

void vdev_pt_init(struct pci_vdev *vdev)
{
	int32_t ret;
	struct acrn_vm *vm = vdev->vpci->vm;
	uint16_t pci_command;

	ASSERT(validate(vdev) == 0, "Error, invalid bar defined");

	/* Create an iommu domain for target VM if not created */
	if (vm->iommu == NULL) {
		if (vm->arch_vm.nworld_eptp == 0UL) {
			vm->arch_vm.nworld_eptp = vm->arch_vm.ept_mem_ops.get_pml4_page(vm->arch_vm.ept_mem_ops.info);
			sanitize_pte((uint64_t *)vm->arch_vm.nworld_eptp);
		}
		vm->iommu = create_iommu_domain(vm->vm_id,
			hva2hpa(vm->arch_vm.nworld_eptp), 48U);
	}

	ret = assign_iommu_device(vm->iommu, (uint8_t)vdev->pdev->bdf.bits.b,
		(uint8_t)(vdev->pdev->bdf.value & 0xFFU));
	if (ret != 0) {
		panic("failed to assign iommu device!");
	}

	pci_command = (uint16_t)pci_pdev_read_cfg(vdev->pdev->bdf, PCIR_COMMAND, 2U);
	/* Disable INTX */
	pci_command |= 0x400U;
	pci_pdev_write_cfg(vdev->pdev->bdf, PCIR_COMMAND, 2U, pci_command);
}

void vdev_pt_deinit(const struct pci_vdev *vdev)
{
	int32_t ret;
	struct acrn_vm *vm = vdev->vpci->vm;

	ret = unassign_iommu_device(vm->iommu, (uint8_t)vdev->pdev->bdf.bits.b,
		(uint8_t)(vdev->pdev->bdf.value & 0xFFU));
	if (ret != 0) {
		panic("failed to unassign iommu device!");
	}
}

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
			vdev->pdev->bar[idx].base, /* HPA */
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

