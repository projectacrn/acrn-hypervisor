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

static struct pci_vdev *partition_mode_find_vdev(struct vpci *vpci, union pci_bdf vbdf)
{
	struct vpci_vdev_array *vdev_array;
	struct pci_vdev *vdev;
	int32_t i;

	vdev_array = vpci->vm->vm_desc->vpci_vdev_array;
	for (i = 0; i < vdev_array->num_pci_vdev; i++) {
		vdev = &vdev_array->vpci_vdev_list[i];
		if (vdev->vbdf.value == vbdf.value) {
			return vdev;
		}
	}

	return NULL;
}

static int32_t partition_mode_vpci_init(struct acrn_vm *vm)
{
	struct vpci_vdev_array *vdev_array;
	struct vpci *vpci = &vm->vpci;
	struct pci_vdev *vdev;
	int32_t i;

	vdev_array = vm->vm_desc->vpci_vdev_array;

	for (i = 0; i < vdev_array->num_pci_vdev; i++) {
		vdev = &vdev_array->vpci_vdev_list[i];
		vdev->vpci = vpci;

		if ((vdev->ops != NULL) && (vdev->ops->init != NULL)) {
			if (vdev->ops->init(vdev) != 0) {
				pr_err("%s() failed at PCI device (bdf %x)!", __func__,
					vdev->vbdf);
			}
		}
	}

	return 0;
}

static void partition_mode_vpci_deinit(struct acrn_vm *vm)
{
	struct vpci_vdev_array *vdev_array;
	struct pci_vdev *vdev;
	int32_t i;

	vdev_array = vm->vm_desc->vpci_vdev_array;

	for (i = 0; i < vdev_array->num_pci_vdev; i++) {
		vdev = &vdev_array->vpci_vdev_list[i];
		if ((vdev->ops != NULL) && (vdev->ops->deinit != NULL)) {
			if (vdev->ops->deinit(vdev) != 0) {
				pr_err("vdev->ops->deinit failed!");
			}
		}
	}
}

static void partition_mode_cfgread(struct vpci *vpci, union pci_bdf vbdf,
	uint32_t offset, uint32_t bytes, uint32_t *val)
{
	struct pci_vdev *vdev = partition_mode_find_vdev(vpci, vbdf);
	if ((vdev != NULL) && (vdev->ops != NULL)
			&& (vdev->ops->cfgread != NULL)) {
		(void)vdev->ops->cfgread(vdev, offset, bytes, val);
	}
}

static void partition_mode_cfgwrite(struct vpci *vpci, union pci_bdf vbdf,
	uint32_t offset, uint32_t bytes, uint32_t val)
{
	struct pci_vdev *vdev = partition_mode_find_vdev(vpci, vbdf);
	if ((vdev != NULL) && (vdev->ops != NULL)
			&& (vdev->ops->cfgwrite != NULL)) {
		(void)vdev->ops->cfgwrite(vdev, offset, bytes, val);
	}
}

struct vpci_ops partition_mode_vpci_ops = {
	.init = partition_mode_vpci_init,
	.deinit = partition_mode_vpci_deinit,
	.cfgread = partition_mode_cfgread,
	.cfgwrite = partition_mode_cfgwrite,
};
