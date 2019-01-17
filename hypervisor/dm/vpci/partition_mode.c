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

/* Virtual PCI device related operations (read/write, etc) */

#include <hypervisor.h>
#include "pci_priv.h"

static struct pci_vdev *partition_mode_find_vdev(struct acrn_vpci *vpci, union pci_bdf vbdf)
{
	struct vpci_vdev_array *vdev_array;
	struct pci_vdev *vdev;
	struct acrn_vm_config *vm_config = get_vm_config(vpci->vm->vm_id);
	int32_t i;

	vdev_array = vm_config->vpci_vdev_array;
	for (i = 0; i < vdev_array->num_pci_vdev; i++) {
		vdev = &vdev_array->vpci_vdev_list[i];
		if (vdev->vbdf.value == vbdf.value) {
			return vdev;
		}
	}

	return NULL;
}

static inline bool is_valid_bar_type(const struct pci_bar *bar)
{
	return (bar->type == PCIBAR_MEM32) || (bar->type == PCIBAR_MEM64);
}

static inline bool is_valid_bar_size(const struct pci_bar *bar)
{
	return (bar->size > 0UL) && (bar->size <= 0xffffffffU);
}

/* Only MMIO is supported and bar size cannot be greater than 4GB */
static inline bool is_valid_bar(const struct pci_bar *bar)
{
	return (is_valid_bar_type(bar) && is_valid_bar_size(bar));
}

static void partition_mode_pdev_init(struct pci_vdev *vdev)
{
	struct pci_pdev *pdev_ref;
	uint32_t idx;
	struct pci_bar *pbar, *vbar;

	pdev_ref = find_pci_pdev(vdev->pdev.bdf);
	if (pdev_ref != NULL) {
		(void)memcpy_s((void *)&vdev->pdev, sizeof(struct pci_pdev), (void *)pdev_ref, sizeof(struct pci_pdev));

		/* Sanity checking for vbar */
		for (idx = 0U; idx < (uint32_t)PCI_BAR_COUNT; idx++) {
			pbar = &vdev->pdev.bar[idx];
			vbar = &vdev->bar[idx];

			if (is_valid_bar(pbar)) {
				vbar->size = (pbar->size < 0x1000U) ? 0x1000U : pbar->size;
				vbar->type = PCIBAR_MEM32;
			} else {
				/* Mark this vbar as invalid */
				vbar->size = 0UL;
				vbar->type = PCIBAR_NONE;
			}
		}
	}
}

static int32_t partition_mode_vpci_init(const struct acrn_vm *vm)
{
	struct vpci_vdev_array *vdev_array;
	const struct acrn_vpci *vpci = &vm->vpci;
	struct pci_vdev *vdev;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	int32_t i;

	vdev_array = vm_config->vpci_vdev_array;

	for (i = 0; i < vdev_array->num_pci_vdev; i++) {
		vdev = &vdev_array->vpci_vdev_list[i];
		vdev->vpci = vpci;

		if (vdev->vbdf.value != 0U) {
			partition_mode_pdev_init(vdev);
			vdev->ops = &pci_ops_vdev_pt;
		} else {
			vdev->ops = &pci_ops_vdev_hostbridge;
		}

		if (vdev->ops->init != NULL) {
			if (vdev->ops->init(vdev) != 0) {
				pr_err("%s() failed at PCI device (bdf %x)!", __func__,
					vdev->vbdf);
			}
		}
	}

	return 0;
}

static void partition_mode_vpci_deinit(const struct acrn_vm *vm)
{
	struct vpci_vdev_array *vdev_array;
	struct pci_vdev *vdev;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	int32_t i;

	vdev_array = vm_config->vpci_vdev_array;

	for (i = 0; i < vdev_array->num_pci_vdev; i++) {
		vdev = &vdev_array->vpci_vdev_list[i];
		if ((vdev->ops != NULL) && (vdev->ops->deinit != NULL)) {
			if (vdev->ops->deinit(vdev) != 0) {
				pr_err("vdev->ops->deinit failed!");
			}
		}
	}
}

static void partition_mode_cfgread(struct acrn_vpci *vpci, union pci_bdf vbdf,
	uint32_t offset, uint32_t bytes, uint32_t *val)
{
	struct pci_vdev *vdev = partition_mode_find_vdev(vpci, vbdf);

	if ((vdev != NULL) && (vdev->ops != NULL)
			&& (vdev->ops->cfgread != NULL)) {
		(void)vdev->ops->cfgread(vdev, offset, bytes, val);
	}
}

static void partition_mode_cfgwrite(struct acrn_vpci *vpci, union pci_bdf vbdf,
	uint32_t offset, uint32_t bytes, uint32_t val)
{
	struct pci_vdev *vdev = partition_mode_find_vdev(vpci, vbdf);

	if ((vdev != NULL) && (vdev->ops != NULL)
			&& (vdev->ops->cfgwrite != NULL)) {
		(void)vdev->ops->cfgwrite(vdev, offset, bytes, val);
	}
}

const struct vpci_ops partition_mode_vpci_ops = {
	.init = partition_mode_vpci_init,
	.deinit = partition_mode_vpci_deinit,
	.cfgread = partition_mode_cfgread,
	.cfgwrite = partition_mode_cfgwrite,
};
