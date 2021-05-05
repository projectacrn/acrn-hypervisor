/*
 * Copyright (c) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pci.h>
#include <asm/guest/vm.h>
#include "vroot_port.h"

#include "vpci_priv.h"

#define PCIE_CAP_VPOS		0x40				/* pcie capability reg position */
#define PTM_CAP_VPOS		PCI_ECAP_BASE_PTR	/* ptm capability reg postion */

static void init_vroot_port(struct pci_vdev *vdev)
{
	/* vendor and device */
	pci_vdev_write_vcfg(vdev, PCIR_VENDOR, 2U, VROOT_PORT_VENDOR);
	pci_vdev_write_vcfg(vdev, PCIR_DEVICE, 2U, VROOT_PORT_DEVICE);

	/* status register */
	pci_vdev_write_vcfg(vdev, PCIR_STATUS, 2U, PCIM_STATUS_CAPPRESENT);

	/* rev id */
	pci_vdev_write_vcfg(vdev, PCIR_REVID, 1U, 0x01U);

	/* sub class */
	pci_vdev_write_vcfg(vdev, PCIR_SUBCLASS, 1U, PCIS_BRIDGE_PCI);

	/* class */
	pci_vdev_write_vcfg(vdev, PCIR_CLASS, 1U, PCIC_BRIDGE);

	/* Header Type */
	pci_vdev_write_vcfg(vdev, PCIR_HDRTYPE, 1U, PCIM_HDRTYPE_BRIDGE);

	/* capability pointer */
	pci_vdev_write_vcfg(vdev, PCIR_CAP_PTR, 1U, PCIE_CAP_VPOS);

	/* pcie capability registers */
	pci_vdev_write_vcfg(vdev, PCIE_CAP_VPOS + PCICAP_ID, 1U, 0x10);
	pci_vdev_write_vcfg(vdev, PCIE_CAP_VPOS + PCICAP_EXP_CAP, 2U, 0x0142);

	vdev->parent_user = NULL;
	vdev->user = vdev;
}

static void deinit_vroot_port(__unused struct pci_vdev *vdev)
{
	vdev->parent_user = NULL;
	vdev->user = NULL;
}

static int32_t read_vroot_port_cfg(const struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t *val)
{
	*val = pci_vdev_read_vcfg(vdev, offset, bytes);

	return 0;
}

static int32_t write_vroot_port_cfg(__unused struct pci_vdev *vdev, __unused uint32_t offset,
	__unused uint32_t bytes, __unused uint32_t val)
{
	pci_vdev_write_vcfg(vdev, offset, bytes, val);

	return 0;
}

const struct pci_vdev_ops vroot_port_ops = {
	.init_vdev         = init_vroot_port,
	.deinit_vdev       = deinit_vroot_port,
	.write_vdev_cfg    = write_vroot_port_cfg,
	.read_vdev_cfg     = read_vroot_port_cfg,
};
