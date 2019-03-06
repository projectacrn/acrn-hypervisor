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

#include <vm.h>
#include <logmsg.h>
#include "pci_priv.h"


static inline bool is_hostbridge(const struct pci_vdev *vdev)
{
	return (vdev->vbdf.value == 0U);
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

/**
 * @pre vdev != NULL
 */
static void partition_mode_pdev_init(struct pci_vdev *vdev, union pci_bdf pbdf)
{
	struct pci_pdev *pdev;
	uint32_t idx;
	struct pci_bar *pbar, *vbar;

	pdev = find_pci_pdev(pbdf);
	ASSERT(pdev != NULL, "pdev is NULL");

	vdev->pdev = pdev;

	/* Sanity checking for vbar */
	for (idx = 0U; idx < (uint32_t)PCI_BAR_COUNT; idx++) {
		pbar = &vdev->pdev->bar[idx];
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

	vdev_pt_init(vdev);
}

/**
 * @pre vm != NULL
 * @pre vm->vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 */
static int32_t partition_mode_vpci_init(const struct acrn_vm *vm)
{
	struct acrn_vpci *vpci = (struct acrn_vpci *)&(vm->vpci);
	struct pci_vdev *vdev;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	struct acrn_vm_pci_ptdev_config *ptdev_config;
	uint32_t i;

	vpci->pci_vdev_cnt = vm_config->pci_ptdev_num;

	for (i = 0U; i < vpci->pci_vdev_cnt; i++) {
		vdev = &vpci->pci_vdevs[i];
		vdev->vpci = vpci;
		ptdev_config = &vm_config->pci_ptdevs[i];
		vdev->vbdf.value = ptdev_config->vbdf.value;

		if (is_hostbridge(vdev)) {
			vdev_hostbridge_init(vdev);
		} else {
			partition_mode_pdev_init(vdev, ptdev_config->pbdf);
		}
	}

	return 0;
}

/**
 * @pre vm != NULL
 * @pre vm->vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 */
static void partition_mode_vpci_deinit(const struct acrn_vm *vm)
{
	struct pci_vdev *vdev;
	uint32_t i;

	for (i = 0U; i < vm->vpci.pci_vdev_cnt; i++) {
		vdev = (struct pci_vdev *) &(vm->vpci.pci_vdevs[i]);

		if (is_hostbridge(vdev)) {
			vdev_hostbridge_deinit(vdev);
		} else {
			vdev_pt_deinit(vdev);
		}
	}
}

static void partition_mode_cfgread(struct acrn_vpci *vpci, union pci_bdf vbdf,
	uint32_t offset, uint32_t bytes, uint32_t *val)
{
	struct pci_vdev *vdev = pci_find_vdev_by_vbdf(vpci, vbdf);

	if (vdev != NULL) {
		if (is_hostbridge(vdev)) {
			if (vdev_hostbridge_cfgread(vdev, offset, bytes, val) != 0) {
				pr_err("vdev_hostbridge_cfgread failed!");
			}
		} else {
			if (vdev_pt_cfgread(vdev, offset, bytes, val) != 0) {
				pr_err("vdev_pt_cfgread failed!");
			}
		}
	}
}

static void partition_mode_cfgwrite(struct acrn_vpci *vpci, union pci_bdf vbdf,
	uint32_t offset, uint32_t bytes, uint32_t val)
{
	struct pci_vdev *vdev = pci_find_vdev_by_vbdf(vpci, vbdf);

	if (vdev != NULL) {
		if (is_hostbridge(vdev)) {
			if (vdev_hostbridge_cfgwrite(vdev, offset, bytes, val) != 0) {
				pr_err("vdev_hostbridge_cfgwrite failed!");
			}
		} else {
			if (vdev_pt_cfgwrite(vdev, offset, bytes, val) != 0){
				pr_err("vdev_pt_cfgwrite failed!");
			}
		}
	}
}

const struct vpci_ops partition_mode_vpci_ops = {
	.init = partition_mode_vpci_init,
	.deinit = partition_mode_vpci_deinit,
	.cfgread = partition_mode_cfgread,
	.cfgwrite = partition_mode_cfgwrite,
};
