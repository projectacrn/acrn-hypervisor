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
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
#include <acrn_hv_defs.h>
#include "pci_priv.h"


static struct pci_vdev *pci_vdev_find(struct vpci *vpci, uint16_t vbdf)
{
	struct vpci_vdev_array *vdev_array;
	struct pci_vdev *vdev;
	int i;

	vdev_array = vpci->vm->vm_desc->vpci_vdev_array;
	for (i = 0; i < vdev_array->num_pci_vdev; i++) {
		vdev = &vdev_array->vpci_vdev_list[i];
		if (vdev->vbdf == vbdf) {
			return vdev;
		}
	}

	return NULL;
}

/* PCI cfg vm-exit handler */
void pci_vdev_cfg_handler(struct vpci *vpci, uint32_t in, uint16_t vbdf,
	uint32_t offset, uint32_t bytes, uint32_t *val)
{
	struct pci_vdev *vdev;
	int ret;

	vdev = pci_vdev_find(vpci, vbdf);
	if (vdev == NULL) {
		return;
	}

	ret = -EINVAL;
	if (in) {
		if ((vdev->ops != NULL) && (vdev->ops->cfgread != NULL)) {
			ret = vdev->ops->cfgread(vdev, offset, bytes, val);
		}
	} else {
		if ((vdev->ops != NULL) && (vdev->ops->cfgwrite != NULL)) {
			ret = vdev->ops->cfgwrite(vdev, offset, bytes, *val);
		}
	}

	if (ret) {
		pr_dbg("pci_vdev_cfg_handler failed, ret=%d", ret);
	}
}

