/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/guest/vm.h>
#include <x86/guest/ept.h>
#include <vpci.h>
#include <logmsg.h>
#include <vmcs9900.h>
#include "vpci_priv.h"
#include <errno.h>

#define MCS9900_MMIO_BAR        0U
#define MCS9900_MSIX_BAR        1U

/*
 * @pre vdev != NULL
 */
void trigger_vmcs9900_msix(struct pci_vdev *vdev)
{
	struct acrn_vm *vm = vpci2vm(vdev->vpci);
	int32_t ret = -1;
	struct msix_table_entry *entry = &vdev->msix.table_entries[0];

	ret = vlapic_inject_msi(vm, entry->addr, entry->data);

	if (ret != 0) {
		pr_warn("%2x:%2x.%dfaild injecting msi msi_addr:0x%lx msi_data:0x%x",
				vdev->bdf.bits.b, vdev->bdf.bits.d, vdev->bdf.bits.f, entry->addr, entry->data);
	}
}

static int32_t read_vmcs9900_cfg(const struct pci_vdev *vdev,
				       uint32_t offset, uint32_t bytes,
				       uint32_t * val)
{
	if (vbar_access(vdev, offset)) {
		*val = pci_vdev_read_vbar(vdev, pci_bar_index(offset));
	} else {
		*val = pci_vdev_read_vcfg(vdev, offset, bytes);
	}

	return 0;
}

static int32_t vmcs9900_mmio_handler(struct io_request *io_req, void *data)
{
	struct mmio_request *mmio = &io_req->reqs.mmio;
	struct pci_vdev *vdev = (struct pci_vdev *)data;
	struct acrn_vuart *vu = vdev->priv_data;
	struct pci_vbar *vbar = &vdev->vbars[MCS9900_MMIO_BAR];
	uint16_t offset;

	offset = mmio->address - vbar->base_gpa;

	if (mmio->direction == REQUEST_READ) {
		mmio->value = vuart_read_reg(vu, offset);
	} else {
		vuart_write_reg(vu, offset, (uint8_t) mmio->value);
	}
	return 0;
}

static void map_vmcs9900_vbar(struct pci_vdev *vdev, uint32_t idx)
{
	struct acrn_vuart *vu = vdev->priv_data;
	struct acrn_vm *vm = vpci2vm(vdev->vpci);
	struct pci_vbar *vbar = &vdev->vbars[idx];

	if ((idx == MCS9900_MMIO_BAR) && (vbar->base_gpa != 0UL)) {
		register_mmio_emulation_handler(vm, vmcs9900_mmio_handler,
			vbar->base_gpa, vbar->base_gpa + vbar->size, vdev, false);
		ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, vbar->base_gpa, vbar->size);
		vu->active = true;
	} else if ((idx == MCS9900_MSIX_BAR) && (vbar->base_gpa != 0UL)) {
		register_mmio_emulation_handler(vm, vmsix_handle_table_mmio_access, vbar->base_gpa,
			(vbar->base_gpa + vbar->size), vdev, false);
		ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, vbar->base_gpa, vbar->size);
		vdev->msix.mmio_gpa = vbar->base_gpa;
	}

}

static void unmap_vmcs9900_vbar(struct pci_vdev *vdev, uint32_t idx)
{
	struct acrn_vuart *vu = vdev->priv_data;
	struct acrn_vm *vm = vpci2vm(vdev->vpci);
	struct pci_vbar *vbar = &vdev->vbars[idx];

	if ((idx == MCS9900_MMIO_BAR) && (vbar->base_gpa != 0UL)) {
		vu->active = false;
	}
	unregister_mmio_emulation_handler(vm, vbar->base_gpa, vbar->base_gpa + vbar->size);
}

static int32_t write_vmcs9900_cfg(struct pci_vdev *vdev, uint32_t offset,
					uint32_t bytes, uint32_t val)
{
	if (vbar_access(vdev, offset)) {
		vpci_update_one_vbar(vdev, pci_bar_index(offset), val,
			map_vmcs9900_vbar, unmap_vmcs9900_vbar);
	} else if (msixcap_access(vdev, offset)) {
		write_vmsix_cap_reg(vdev, offset, bytes, val);
	} else {
		pci_vdev_write_vcfg(vdev, offset, bytes, val);
	}

	return 0;
}

static void init_vmcs9900(struct pci_vdev *vdev)
{
	struct acrn_vm_pci_dev_config *pci_cfg = vdev->pci_dev_config;
	struct acrn_vm *vm = vpci2vm(vdev->vpci);
	struct pci_vbar *mmio_vbar = &vdev->vbars[MCS9900_MMIO_BAR];
	struct pci_vbar *msix_vbar = &vdev->vbars[MCS9900_MSIX_BAR];
	struct acrn_vuart *vu = &vm->vuart[pci_cfg->vuart_idx];

	/* 8250-pci compartiable device */
	pci_vdev_write_vcfg(vdev, PCIR_VENDOR, 2U, MCS9900_VENDOR);
	pci_vdev_write_vcfg(vdev, PCIR_DEVICE, 2U, MCS9900_DEV);
	pci_vdev_write_vcfg(vdev, PCIR_CLASS, 1U, PCIC_SIMPLECOMM);
	pci_vdev_write_vcfg(vdev, PCIV_SUB_SYSTEM_ID, 2U, 0x1000U);
	pci_vdev_write_vcfg(vdev, PCIV_SUB_VENDOR_ID, 2U, 0xa000U);
	pci_vdev_write_vcfg(vdev, PCIR_SUBCLASS, 1U, 0x0U);
	pci_vdev_write_vcfg(vdev, PCIR_CLASS_CODE, 1U, 0x2U);

	add_vmsix_capability(vdev, 1, MCS9900_MSIX_BAR);

	/* initialize vuart-pci mem bar */
	mmio_vbar->size = 0x1000U;
	mmio_vbar->base_gpa = pci_cfg->vbar_base[MCS9900_MMIO_BAR];
	mmio_vbar->mask = (uint32_t) (~(mmio_vbar->size - 1UL));
	mmio_vbar->bar_type.bits = PCIM_BAR_MEM_32;

	/* initialize vuart-pci msix bar */
	msix_vbar->size = 0x1000U;
	msix_vbar->base_gpa = pci_cfg->vbar_base[MCS9900_MSIX_BAR];
	msix_vbar->mask = (uint32_t) (~(msix_vbar->size - 1UL));
	msix_vbar->bar_type.bits = PCIM_BAR_MEM_32;

	vdev->nr_bars = 2;

	pci_vdev_write_vbar(vdev, MCS9900_MMIO_BAR, mmio_vbar->base_gpa);
	pci_vdev_write_vbar(vdev, MCS9900_MSIX_BAR, msix_vbar->base_gpa);

	/* init acrn_vuart */
	pr_info("init acrn_vuart[%d]", pci_cfg->vuart_idx);
	vdev->priv_data = vu;
	init_pci_vuart(vdev);

	vdev->user = vdev;
}

static void deinit_vmcs9900(struct pci_vdev *vdev)
{
	deinit_pci_vuart(vdev);
	vdev->user = NULL;
}

const struct pci_vdev_ops vmcs9900_ops = {
	.init_vdev = init_vmcs9900,
	.deinit_vdev = deinit_vmcs9900,
	.write_vdev_cfg = write_vmcs9900_cfg,
	.read_vdev_cfg = read_vmcs9900_cfg,
};

int32_t create_vmcs9900_vdev(struct acrn_vm *vm, struct acrn_emul_dev *dev)
{
	uint32_t i;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	struct acrn_vm_pci_dev_config *dev_config = NULL;
	int32_t ret = -EINVAL;
	uint16_t vuart_idx = *((uint16_t*)(dev->args));

	for (i = 0U; i < vm_config->pci_dev_num; i++) {
		dev_config = &vm_config->pci_devs[i];
		if (dev_config->vuart_idx == vuart_idx) {
			dev_config->vbdf.value = (uint16_t) dev->slot;
			dev_config->vbar_base[0] = (uint64_t) dev->io_addr[0];
			dev_config->vbar_base[1] = (uint64_t) dev->io_addr[1];
			(void) vpci_init_vdev(&vm->vpci, dev_config, NULL);
			ret = 0;
			break;
		}
	}

	if (ret != 0) {
		pr_err("Unsupport: create VM%d vuart_idx=%d", vm->vm_id, vuart_idx);
	}

	return ret;
}

int32_t destroy_vmcs9900_vdev(struct pci_vdev *vdev)
{
	uint32_t i;

	for (i = 0U; i < vdev->nr_bars; i++) {
		vpci_update_one_vbar(vdev, i, 0U, NULL, unmap_vmcs9900_vbar);
	}

	deinit_pci_vuart(vdev);

	return 0;
}
