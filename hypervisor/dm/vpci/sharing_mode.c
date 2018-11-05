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

static uint32_t num_pci_vdev;
static struct pci_vdev sharing_mode_vdev_array[CONFIG_MAX_PCI_DEV_NUM];

struct pci_vdev *sharing_mode_find_vdev(union pci_bdf pbdf)
{
	uint32_t i;

	/* in VM0, it uses phys BDF */
	for (i = 0U; i < num_pci_vdev; i++) {
		if (sharing_mode_vdev_array[i].pdev.bdf.value == pbdf.value) {
			return &sharing_mode_vdev_array[i];
		}
	}

	return NULL;
}

static void sharing_mode_cfgread(__unused struct vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t *val)
{
	struct pci_vdev *vdev;
	bool handled = false;
	uint32_t i;

	vdev = sharing_mode_find_vdev(bdf);

	/* vdev == NULL: Could be hit for PCI enumeration from guests */
	if ((vdev == NULL) || ((bytes != 1U) && (bytes != 2U) && (bytes != 4U))) {
		*val = ~0U;
		return;
	}

	for (i = 0U; (i < vdev->nr_ops) && !handled; i++) {
		if (vdev->ops[i].cfgread != NULL) {
			if (vdev->ops[i].cfgread(vdev, offset, bytes, val) == 0) {
				handled = true;
			}
		}
	}

	/* Not handled by any handlers. Passthru to physical device */
	if (!handled) {
		*val = pci_pdev_read_cfg(vdev->pdev.bdf, offset, bytes);
	}
}

static void sharing_mode_cfgwrite(__unused struct vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t val)
{
	struct pci_vdev *vdev;
	bool handled = false;
	uint32_t i;

	if ((bytes != 1U) && (bytes != 2U) && (bytes != 4U)) {
		return;
	}

	vdev = sharing_mode_find_vdev(bdf);
	if (vdev != NULL) {
		for (i = 0U; (i < vdev->nr_ops) && !handled; i++) {
			if (vdev->ops[i].cfgwrite != NULL) {
				if (vdev->ops[i].cfgwrite(vdev, offset, bytes, val) == 0) {
					handled = true;
				}
			}
		}

		/* Not handled by any handlers. Passthru to physical device */
		if (!handled) {
			pci_pdev_write_cfg(vdev->pdev.bdf, offset, bytes, val);
		}
	}
}

static struct pci_vdev *alloc_pci_vdev(struct acrn_vm *vm, union pci_bdf bdf)
{
	struct pci_vdev *vdev;

	if (num_pci_vdev >= CONFIG_MAX_PCI_DEV_NUM) {
		return NULL;
	}

	vdev = &sharing_mode_vdev_array[num_pci_vdev++];

	/* vbdf equals to pbdf otherwise remapped */
	vdev->vbdf = bdf;
	vdev->vpci = &vm->vpci;
	vdev->pdev.bdf = bdf;

	return vdev;
}

static void enumerate_pci_dev(uint16_t pbdf, void *cb_data)
{
	struct acrn_vm *vm = (struct acrn_vm *)cb_data;
	struct pci_vdev *vdev;

	vdev = alloc_pci_vdev(vm, (union pci_bdf)pbdf);
	if (vdev != NULL) {
		populate_msi_struct(vdev);
	}
}

static int sharing_mode_vpci_init(struct acrn_vm *vm)
{
	struct pci_vdev *vdev;
	uint32_t i, j;

	/*
	 * Only setup IO bitmap for SOS.
	 * IO/MMIO requests from non-vm0 guests will be injected to device model.
	 */
	if (!is_vm0(vm)) {
		return -ENODEV;
	}

	/* Initialize PCI vdev array */
	num_pci_vdev = 0U;
	(void)memset((void *)sharing_mode_vdev_array, 0U, sizeof(sharing_mode_vdev_array));

	/* build up vdev array for vm0 */
	pci_scan_bus(enumerate_pci_dev, (void *)vm);

	for (i = 0U; i < num_pci_vdev; i++) {
		vdev = &sharing_mode_vdev_array[i];
		for (j = 0U; j < vdev->nr_ops; j++) {
			if (vdev->ops[j].init != NULL) {
				(void)vdev->ops[j].init(vdev);
			}
		}
	}

	return 0;
}

static void sharing_mode_vpci_deinit(__unused struct acrn_vm *vm)
{
	struct pci_vdev *vdev;
	uint32_t i, j;

	if (!is_vm0(vm)) {
		return;
	}

	for (i = 0U; i < num_pci_vdev; i++) {
		vdev = &sharing_mode_vdev_array[i];
		for (j = 0U; j < vdev->nr_ops; j++) {
			if (vdev->ops[j].deinit != NULL) {
				(void)vdev->ops[j].deinit(vdev);
			}
		}
	}
}

void add_vdev_handler(struct pci_vdev *vdev, struct pci_vdev_ops *ops)
{
	if (vdev->nr_ops >= (MAX_VPCI_DEV_OPS - 1U)) {
		pr_err("%s, adding too many handlers", __func__);
		return;
	}

	vdev->ops[vdev->nr_ops++] = *ops;
}

struct vpci_ops sharing_mode_vpci_ops = {
	.init = sharing_mode_vpci_init,
	.deinit = sharing_mode_vpci_deinit,
	.cfgread = sharing_mode_cfgread,
	.cfgwrite = sharing_mode_cfgwrite,
};

void vpci_set_ptdev_intr_info(struct acrn_vm *target_vm, uint16_t vbdf, uint16_t pbdf)
{
	struct pci_vdev *vdev;

	vdev = sharing_mode_find_vdev((union pci_bdf)pbdf);
	if (vdev == NULL) {
		pr_err("%s, can't find PCI device for vm%d, vbdf (0x%x) pbdf (0x%x)", __func__,
			target_vm->vm_id, vbdf, pbdf);
		return;
	}

	/* UOS may do BDF mapping */
	vdev->vpci = &target_vm->vpci;
	vdev->vbdf.value = vbdf;
	vdev->pdev.bdf.value = pbdf;
}

void vpci_reset_ptdev_intr_info(struct acrn_vm *target_vm, uint16_t vbdf, uint16_t pbdf)
{
	struct pci_vdev *vdev;
	struct acrn_vm *vm;

	vdev = sharing_mode_find_vdev((union pci_bdf)pbdf);
	if (vdev == NULL) {
		pr_err("%s, can't find PCI device for vm%d, vbdf (0x%x) pbdf (0x%x)", __func__,
			target_vm->vm_id, vbdf, pbdf);
		return;
	}

	/* Return this PCI device to SOS */
	if (vdev->vpci->vm == target_vm) {
		vm = get_vm_from_vmid(0U);
		vdev->vpci = &vm->vpci;
	}
}
