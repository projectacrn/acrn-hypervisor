/*
 * Copyright (C) 2021-2022 Intel Corporation.
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
get_ptm_reg_value(struct pci_device *pdev, int reg)
{
	int pos;
	uint32_t reg_val;

	pos = pci_find_ext_cap(pdev, PCIZ_PTM);

	if (!pos) {
		return 0;
	}

	pci_device_cfg_read_u32(pdev, &reg_val, pos + reg);

	pr_notice("<PTM>-%s: device [%x:%x.%x]: ptm pos=0x%x, ptm reg val=0x%x.\n",
				__func__, pdev->bus, pdev->dev, pdev->func, pos, reg_val);

	return reg_val;
}

/* add virtual root port to hv */
static int
add_vroot_port(struct vmctx *ctx, struct passthru_dev *ptdev, struct pci_device *root_port, int ptm_cap_offset)
{
	int error = 0;
	int offset = 0;
	uint32_t dev_cap = 0;

	struct acrn_vdev rp_vdev = {};
	struct vrp_config *rp_priv = (struct vrp_config *)&rp_vdev.args;

	rp_vdev.id.fields.vendor = PTM_ROOT_PORT_VENDOR;
	rp_vdev.id.fields.device = PTM_ROOT_PORT_DEVICE;

	// virtual root port takes bdf from its downstream device
	rp_vdev.slot = PCI_BDF(ptdev->dev->bus, ptdev->dev->slot, ptdev->dev->func);

	rp_priv->phy_bdf = PCI_BDF(root_port->bus, root_port->dev, root_port->func);

	rp_priv->primary_bus = ptdev->dev->bus;

	rp_priv->secondary_bus = ++ptm_root_port_secondary_bus;

	// only passthru device is connected to virtual root port
	rp_priv->subordinate_bus = rp_priv->secondary_bus;

	rp_priv->ptm_capable = 1;

	rp_priv->ptm_cap_offset = ptm_cap_offset;

	/* It seems important that passthru device's max payload settings match
	 * the settings on the native device otherwise passthru device may not work.
	 * So we have to set vrp's max payload capacity the same as native root port
	 * otherwise we may accidentally change passthru device's max payload since
	 * during guest OS's pci enumeration, pass-thru device will renegotiate
	 * its max payload's setting with vrp.
	 */
	offset = pci_find_cap(root_port, PCIY_EXPRESS);
	pci_device_cfg_read_u32(root_port, &dev_cap, offset + PCIER_DEVICE_CAP);
	rp_priv->max_payload = dev_cap & PCIEM_CAP_MAX_PAYLOAD;
	pr_info("%s: virtual root port info: vbdf=0x%x, phy_bdf=0x%x, prim_bus=%x, sec_bus=%x, sub_bus=%x, ptm_cpa_offset=0x%x, max_payload=0x%x.\n",
		 __func__, rp_vdev.slot, rp_priv->phy_bdf, rp_priv->primary_bus, rp_priv->secondary_bus, rp_priv->subordinate_bus,
		 rp_priv->ptm_cap_offset, rp_priv->max_payload);

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
	int pos, pcie_type, cap, rp_ptm_offset;
	struct pci_device *phys_dev = ptdev->phys_dev;
	struct pci_device *rp;

	*vrp_sec_bus = 0;

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

	/* The following sanity checks are based on these assumptions:
	 * 1. PTM requestor can be enabled on pci ep or rcie.
	 * 2. HW implements this simple PTM hierarchy: PTM requestor (EP) is
	 * directly connected to PTM root (root port), or requestor itself is RCIE.
	 * 3. There is no intermediate nodes (such as switch) in between the PTM
	 * root and PTM requestor.
	 * 4. HW only implements one PCI domain in the system and only one PTM
	 * domain implemented in this PCI domain
	 */
	if (pcie_type == PCIEM_TYPE_ENDPOINT) {
		cap = get_ptm_reg_value(phys_dev, PCIR_PTM_CAP);
		if (!(cap & PCIM_PTM_CAP_REQ)) {
			pr_err("%s Error: %x:%x.%x must be PTM requestor.\n", __func__,
					phys_dev->bus, phys_dev->dev, phys_dev->func);
			return -EINVAL;
		}

		/* Do not support switch */
		rp = pci_device_get_parent_bridge(phys_dev);
		if ((rp == NULL) || !is_root_port(rp)) {
			pr_err("%s Error: Cannot find root port of %x:%x.%x.\n", __func__,
					phys_dev->bus, phys_dev->dev, phys_dev->func);
			return -ENODEV;
		}

		/* check whether root port is PTM root-capable */
		cap = get_ptm_reg_value(rp, PCIR_PTM_CAP);
		if (!(cap & PCIM_PTM_CAP_ROOT)) {
			pr_err("%s Error: root port %x:%x.%x of %x:%x.%x is not PTM root capable.\n",
				__func__, rp->bus, rp->dev, rp->func,
				phys_dev->bus, phys_dev->dev, phys_dev->func);
			return -EINVAL;
		}

		/* TODO: Support multiple PFs device later as it needs to consider to prevent P2P
		 * attack through ACS or passthrough all PFs together.
		 */
		if (is_mfdev(phys_dev)) {
			pr_err("%s: Failed to enable PTM on root port [%x:%x.%x], multi-func dev is not supported.\n",
					__func__, rp->bus, rp->dev, rp->func);
			return -EINVAL;
		}

		/* hv is responsible to ensure that PTM is enabled on hw root port if
		 * root port is PTM root-capable.  If PTM root is not enabled already in physical
		 * root port before guest launch, guest OS can only enable it in root port's virtual
		 * config space and PTM may not function as desired so we are not going to allow user
		 * to enable PTM on pass-thru device.
		 */
		cap = get_ptm_reg_value(rp, PCIR_PTM_CTRL);
		if (!(cap & PCIM_PTM_CTRL_ENABLE) || !(cap & PCIM_PTM_CTRL_ROOT_SELECT)) {
			pr_err("%s Warning: guest is not allowed to enable PTM on root port %x:%x.%x.\n",
				__func__, rp->bus, rp->dev, rp->func);

			return -EINVAL;
		}

		rp_ptm_offset = pci_find_ext_cap(rp, PCIZ_PTM);

		/* add virtual root port */
		*vrp_sec_bus = add_vroot_port(ctx, ptdev, rp, rp_ptm_offset);
	} else if (pcie_type == PCIEM_TYPE_ROOT_INT_EP) {
		/* Do NOT emulate root port if ptm requestor is RCIE */
		pr_notice("%s: ptm requestor is root complex integrated device.\n", __func__);
	} else {
		pr_err("%s Error: PTM can only be enabled on pci root complex integrated device or endpoint device.\n",
					__func__);
		return -EINVAL;
	}

	return 0;
}
