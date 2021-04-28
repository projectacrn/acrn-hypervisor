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

/* Probe whether device and its root port support PTM */
int ptm_probe(struct vmctx *ctx, struct passthru_dev *ptdev)
{
	int pos, pcie_type, cap, rootport_ptm_offset, device_ptm_offset;
	struct pci_device *phys_dev = ptdev->phys_dev;
	struct pci_device *root_port;

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

		root_port = pci_find_root_port(phys_dev);
		if (root_port == NULL) {
			pr_err("%s Error: Cannot find root port of %x:%x.%x.\n", __func__,
					phys_dev->bus, phys_dev->dev, phys_dev->func);
			return -ENODEV;
		}

		cap = get_ptm_cap(root_port, &rootport_ptm_offset);
		if (!(cap & PCIM_PTM_CAP_ROOT)) {
			pr_err("%s Error: root port %x:%x.%x of %x:%x.%x is not PTM root.\n",
				__func__, root_port->bus, root_port->dev,
				root_port->func, phys_dev->bus, phys_dev->dev, phys_dev->func);
			return -EINVAL;
		}
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
