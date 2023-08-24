/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <errno.h>
#include <logmsg.h>
#include <pci.h>
#include "vpci_priv.h"

/* config space of dummy multifunction device */
#define PCI_DUMMY_DEVICE_VENDOR		0x1D94U
#define PCI_DUMMY_DEVICE_ID		0x145AU
#define DUMMY_MF_REV			0x1U
#define DUMMY_MF_CLASS			0x0U

static void init_vpci_mf_dev(struct pci_vdev *vdev)
{
	pci_vdev_write_vcfg(vdev, PCIR_VENDOR, 2U, PCI_DUMMY_DEVICE_VENDOR);
	pci_vdev_write_vcfg(vdev, PCIR_DEVICE, 2U, PCI_DUMMY_DEVICE_ID);
	pci_vdev_write_vcfg(vdev, PCIR_REVID, 1U, DUMMY_MF_REV);
	pci_vdev_write_vcfg(vdev, PCIR_CLASS, 1U, DUMMY_MF_CLASS);
	pci_vdev_write_vcfg(vdev, PCIR_HDRTYPE, 1U, PCIM_HDRTYPE_NORMAL | PCIM_MFDEV);

	vdev->parent_user = NULL;
	vdev->user = vdev;
}

static void deinit_vpci_mf_dev(struct pci_vdev *vdev)
{
	vdev->parent_user = NULL;
	vdev->user = NULL;
}

static int32_t read_vpci_mf_dev(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t *val)
{
	*val = pci_vdev_read_vcfg(vdev, offset, bytes);

	return 0;
}

static int32_t write_vpci_mf_dev(__unused struct pci_vdev *vdev, __unused uint32_t offset,
	__unused uint32_t bytes, __unused uint32_t val)
{
	return 0;
}

const struct pci_vdev_ops vpci_mf_dev_ops = {
	.init_vdev         = init_vpci_mf_dev,
	.deinit_vdev       = deinit_vpci_mf_dev,
	.write_vdev_cfg    = write_vpci_mf_dev,
	.read_vdev_cfg     = read_vpci_mf_dev,
};
