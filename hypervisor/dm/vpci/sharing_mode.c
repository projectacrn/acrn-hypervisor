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

#include <vm.h>
#include <errno.h>
#include <logmsg.h>
#include "pci_priv.h"


static struct pci_vdev *sharing_mode_find_vdev_sos(union pci_bdf pbdf)
{
	struct acrn_vm *vm;

	vm = get_sos_vm();

	return pci_find_vdev_by_pbdf(&vm->vpci, pbdf);
}

static void sharing_mode_cfgread(__unused struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t *val)
{
	struct pci_vdev *vdev;

	vdev = sharing_mode_find_vdev_sos(bdf);

	/* vdev == NULL: Could be hit for PCI enumeration from guests */
	if ((vdev == NULL) || ((bytes != 1U) && (bytes != 2U) && (bytes != 4U))) {
		*val = ~0U;
	} else {
		if ((vmsi_cfgread(vdev, offset, bytes, val) != 0)
			&& (vmsix_cfgread(vdev, offset, bytes, val) != 0)
			) {
			/* Not handled by any handlers, passthru to physical device */
			*val = pci_pdev_read_cfg(vdev->pdev->bdf, offset, bytes);
		}
	}
}

static void sharing_mode_cfgwrite(__unused struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t val)
{
	struct pci_vdev *vdev;

	if ((bytes == 1U) || (bytes == 2U) || (bytes == 4U)) {
		vdev = sharing_mode_find_vdev_sos(bdf);
		if (vdev != NULL) {
			if ((vmsi_cfgwrite(vdev, offset, bytes, val) != 0)
				&& (vmsix_cfgwrite(vdev, offset, bytes, val) != 0)
				) {
				/* Not handled by any handlers, passthru to physical device */
				pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, val);
			}
		}
	}
}

/**
 * @pre pdev != NULL
 * @pre vm != NULL
 * @pre vm->vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 */
static void init_vdev_for_pdev(struct pci_pdev *pdev, const void *vm)
{
	struct pci_vdev *vdev = NULL;
	struct acrn_vpci *vpci = &(((struct acrn_vm *)vm)->vpci);

	if (vpci->pci_vdev_cnt < CONFIG_MAX_PCI_DEV_NUM) {
		vdev = &vpci->pci_vdevs[vpci->pci_vdev_cnt];
		vpci->pci_vdev_cnt++;

		vdev->vpci = vpci;
		/* vbdf equals to pbdf otherwise remapped */
		vdev->vbdf = pdev->bdf;
		vdev->pdev = pdev;

		vmsi_init(vdev);

		vmsix_init(vdev);
	}
}

static int32_t sharing_mode_vpci_init(const struct acrn_vm *vm)
{
	int32_t ret = -ENODEV;

	/*
	 * Only set up IO bitmap for SOS.
	 * IO/MMIO requests from non-sos_vm guests will be injected to device model.
	 */
	if (is_sos_vm(vm)) {
		/* Build up vdev array for sos_vm */
		pci_pdev_foreach(init_vdev_for_pdev, vm);
		ret = 0;
	}

	return ret;
}

/**
 * @pre vm != NULL
 * @pre vm->vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 */
static void sharing_mode_vpci_deinit(const struct acrn_vm *vm)
{
	struct pci_vdev *vdev;
	uint32_t i;

	if (is_sos_vm(vm)) {
		for (i = 0U; i < vm->vpci.pci_vdev_cnt; i++) {
			vdev = (struct pci_vdev *)&(vm->vpci.pci_vdevs[i]);

			vmsi_deinit(vdev);

			vmsix_deinit(vdev);
		}
	}
}

const struct vpci_ops sharing_mode_vpci_ops = {
	.init = sharing_mode_vpci_init,
	.deinit = sharing_mode_vpci_deinit,
	.cfgread = sharing_mode_cfgread,
	.cfgwrite = sharing_mode_cfgwrite,
};

void vpci_set_ptdev_intr_info(const struct acrn_vm *target_vm, uint16_t vbdf, uint16_t pbdf)
{
	struct pci_vdev *vdev;
	union pci_bdf bdf;

	bdf.value = pbdf;
	vdev = sharing_mode_find_vdev_sos(bdf);
	if (vdev == NULL) {
		pr_err("%s, can't find PCI device for vm%d, vbdf (0x%x) pbdf (0x%x)", __func__,
			target_vm->vm_id, vbdf, pbdf);
	} else {
		/* UOS may do BDF mapping */
		vdev->vpci = (struct acrn_vpci *)&(target_vm->vpci);
		vdev->vbdf.value = vbdf;
		vdev->pdev->bdf.value = pbdf;
	}
}

void vpci_reset_ptdev_intr_info(const struct acrn_vm *target_vm, uint16_t vbdf, uint16_t pbdf)
{
	struct pci_vdev *vdev;
	struct acrn_vm *vm;
	union pci_bdf bdf;

	bdf.value = pbdf;
	vdev = sharing_mode_find_vdev_sos(bdf);
	if (vdev == NULL) {
		pr_err("%s, can't find PCI device for vm%d, vbdf (0x%x) pbdf (0x%x)", __func__,
			target_vm->vm_id, vbdf, pbdf);
	} else {
		/* Return this PCI device to SOS */
		if (vdev->vpci->vm == target_vm) {
			vm = get_sos_vm();

			if (vm != NULL) {
				vdev->vpci = &vm->vpci;
				/* vbdf equals to pbdf in sos */
				vdev->vbdf.value = vdev->pdev->bdf.value;
			}
		}
	}
}
