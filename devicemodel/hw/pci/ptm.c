/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "pcireg.h"
#include "pciaccess.h"
#include "pci_core.h"
#include "ptm.h"
#include "passthru.h"
#include "pci_util.h"
#include "vmmapi.h"
#include "acrn_common.h"

#define PTM_ROOT_PORT_VENDOR           0x8086U
#define PTM_ROOT_PORT_DEVICE           0x9d14U

/* PTM capability register ID*/
#define PCIZ_PTM    0x1fU

/* PTM register Definitions */
/* PTM capability register */
#define PCIR_PTM_CAP				0x04U
	#define PCIM_PTM_CAP_REQ		0x01U	/* Requestor capable */
	#define PCIM_PTM_CAP_ROOT		0x4U	/* Root capable */
	#define PCIM_PTM_GRANULARITY	0xFF00	/* Clock granularity */
/* PTM control register */
#define PCIR_PTM_CTRL				0x08U
	#define PCIM_PTM_CTRL_ENABLE	0x1U	/* PTM enable */
	#define PCIM_PTM_CTRL_ROOT_SELECT	0x2U	/* Root select */

/* virtual root port secondary bus */
static int ptm_root_port_secondary_bus;

/* get ptm capability register value */
static int
get_ptm_cap(struct pci_device *pdev, int *offset)
{
	int pos;
	uint32_t cap;

	pos = pci_find_ext_cap(pdev, PCIZ_PTM);

	if (!pos) {
		*offset = 0;
		return 0;
	}

	*offset = pos;
	pci_device_cfg_read_u32(pdev, &cap, pos + PCIR_PTM_CAP);

	pr_notice("<PTM>-%s: device [%x:%x.%x]: ptm-cap=0x%x, offset=0x%x.\n",
				__func__, pdev->bus, pdev->dev, pdev->func, cap, *offset);

	return cap;
}

/* add virtual root port to hv */
static int
add_vroot_port(struct vmctx *ctx, struct passthru_dev *ptdev, struct pci_device *root_port, int ptm_cap_offset)
{
	int error = 0;

	struct acrn_emul_dev rp_vdev = {};
	struct vrp_config *rp_priv = (struct vrp_config *)&rp_vdev.args;

	rp_vdev.dev_id.fields.vendor_id = PTM_ROOT_PORT_VENDOR;
	rp_vdev.dev_id.fields.device_id = PTM_ROOT_PORT_DEVICE;

	// virtual root port takes bdf from its downstream device
	rp_vdev.slot = PCI_BDF(ptdev->dev->bus, ptdev->dev->slot, ptdev->dev->func);

	rp_priv->phy_bdf = PCI_BDF(root_port->bus, root_port->dev, root_port->func);

	rp_priv->primary_bus = ptdev->dev->bus;

	rp_priv->secondary_bus = ++ptm_root_port_secondary_bus;

	// only passthru device is connected to virtual root port
	rp_priv->subordinate_bus = rp_priv->secondary_bus;

	rp_priv->ptm_capable = 1;

	rp_priv->ptm_cap_offset = ptm_cap_offset;

	pr_info("%s: virtual root port info: vbdf=0x%x, phy_bdf=0x%x, prim_bus=%x, sec_bus=%x, sub_bus=%x, ptm_cpa_offset=0x%x.\n", __func__,
		rp_vdev.slot, rp_priv->phy_bdf, rp_priv->primary_bus, rp_priv->secondary_bus, rp_priv->subordinate_bus, rp_priv->ptm_cap_offset);

	error = vm_add_hv_vdev(ctx, &rp_vdev);
	if (error) {
		pr_err("failed to add virtual root.\n");
		return -1;
	} else
		return rp_priv->secondary_bus;
}

/* Probe whether device and its root port support PTM */
int ptm_probe(struct vmctx *ctx, struct passthru_dev *ptdev, int *vrp_sec_bus)
{
	int error = 0;
	int pos, pcie_type, cap, rp_ptm_offset, device_ptm_offset;
	struct pci_device *phys_dev = ptdev->phys_dev;
	struct pci_device *rp;
	struct pci_device_info rp_info = {};

	*vrp_sec_bus = 0;

	/* build pci hierarch */
	error = scan_pci();
	if (error)
		return error;

	if (!ptdev->pcie_cap) {
		pr_err("%s Error: %x:%x.%x is not a pci-e device.\n", __func__,
				phys_dev->bus, phys_dev->dev, phys_dev->func);
		return -EINVAL;
	}

	pos = pci_find_ext_cap(phys_dev, PCIZ_PTM);
	if (!pos) {
		pr_err("%s Error: %x:%x.%x doesn't support ptm.\n", __func__,
				phys_dev->bus, phys_dev->dev, phys_dev->func);
		return -EINVAL;
	}

	pcie_type = pci_get_pcie_type(phys_dev);

	/* Assume that PTM requestor can be enabled on pci ep or rcie.
	 * If PTM is enabled on an ep, PTM hierarchy requires it has an upstream
	 * PTM root.  Based on available HW platforms, currently PTM root
	 * capability is implemented in pci root port.
	 */
	if (pcie_type == PCIEM_TYPE_ENDPOINT) {
		cap = get_ptm_cap(phys_dev, &device_ptm_offset);
		if (!(cap & PCIM_PTM_CAP_REQ)) {
			pr_err("%s Error: %x:%x.%x must be PTM requestor.\n", __func__,
					phys_dev->bus, phys_dev->dev, phys_dev->func);
			return -EINVAL;
		}

		rp = pci_find_root_port(phys_dev);
		if (rp == NULL) {
			pr_err("%s Error: Cannot find root port of %x:%x.%x.\n", __func__,
					phys_dev->bus, phys_dev->dev, phys_dev->func);
			return -ENODEV;
		}

		cap = get_ptm_cap(rp, &rp_ptm_offset);
		if (!(cap & PCIM_PTM_CAP_ROOT)) {
			pr_err("%s Error: root port %x:%x.%x of %x:%x.%x is not PTM root.\n",
				__func__, rp->bus, rp->dev,
				rp->func, phys_dev->bus, phys_dev->dev, phys_dev->func);
			return -EINVAL;
		}

		/* check whether more than one devices are connected to the root port.
		 * if more than one devices are connected to the root port, we flag
		 * this as error and won't enable PTM.  We do this just because we
		 * don't have this hw configuration and won't be able totest this case.
		 */
		error = pci_device_get_bridge_buses(rp, &(rp_info.primary_bus),
					&(rp_info.secondary_bus), &(rp_info.subordinate_bus));

		if (error || (get_device_count_on_bridge(&rp_info) != 1)) {
			pr_err("%s Error: Failed to enable PTM on root port [%x:%x.%x] that has multiple children.\n",
					__func__, rp->bus, rp->dev, rp->func);
			return -EINVAL;
		}

		// add virtual root port
		*vrp_sec_bus = add_vroot_port(ctx, ptdev, rp, rp_ptm_offset);

	} else if (pcie_type == PCIEM_TYPE_ROOT_INT_EP) {
		// No need to emulate root port if ptm requestor is RCIE
		pr_notice("%s: ptm requestor is root complex integrated device.\n",
					__func__);
	} else {
		pr_err("%s Error: PTM can only be enabled on pci root complex integrated device or endpoint device.\n",
					__func__);
		return -EINVAL;
	}

	return 0;
}
