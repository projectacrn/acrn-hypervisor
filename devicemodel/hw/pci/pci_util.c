/*
 * Copyright (C) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <errno.h>
#include <stddef.h>
#include <sys/queue.h>
#include <stdlib.h>

#include "pcireg.h"
#include "pciaccess.h"
#include "pci_core.h"
#include "pci_util.h"
#include "log.h"


/* find position of specified pci capability register*/
int pci_find_cap(struct pci_device *pdev, const int cap_id)
{
	uint8_t cap_pos, cap_data;
	uint16_t status = 0;

	pci_device_cfg_read_u16(pdev, &status, PCIR_STATUS);
	if (status & PCIM_STATUS_CAPPRESENT) {
		pci_device_cfg_read_u8(pdev, &cap_pos, PCIR_CAP_PTR);

		while (cap_pos != 0 && cap_pos != 0xff) {
			pci_device_cfg_read_u8(pdev, &cap_data,
					cap_pos + PCICAP_ID);

			if (cap_data == cap_id)
				return cap_pos;

			pci_device_cfg_read_u8(pdev, &cap_pos,
					cap_pos + PCICAP_NEXTPTR);
		}
	}

	return 0;
}

/* find extend capability register position from cap_id */
int pci_find_ext_cap(struct pci_device *pdev, int cap_id)
{
	int offset = 0;
	uint32_t data = 0;

	offset = PCIR_EXTCAP;

	do {
		/* PCI Express Extended Capability must have 4 bytes header */
		pci_device_cfg_read_u32(pdev, &data, offset);

		if (PCI_EXTCAP_ID(data) == cap_id)
			break;

		offset = PCI_EXTCAP_NEXTPTR(data);
	} while (offset != 0);

	return offset;
}

/* find pci-e device type */
int pci_get_pcie_type(struct pci_device *dev)
{
	uint8_t data = 0;
	int pcie_type;
	int pos = 0;

	if (dev == NULL)
		return -EINVAL;

	pos = pci_find_cap(dev, PCIY_EXPRESS);
	if (!pos)
		return -EINVAL;

	pci_device_cfg_read_u8(dev, &data, pos + PCIER_FLAGS);
	pcie_type = data & PCIEM_FLAGS_TYPE;

	return pcie_type;
}

/* check whether pdev is a pci root port */
bool is_root_port(struct pci_device *pdev)
{
	int pcie_type;

	pcie_type = pci_get_pcie_type(pdev);

	return (pcie_type == PCIEM_TYPE_ROOT_PORT);
}

/* check whether pdev is multi-function device */
bool is_mfdev(struct pci_device *pdev)
{
	uint8_t hdr_type;

	pci_device_cfg_read_u8(pdev, &hdr_type, PCIR_HDRTYPE);

	return ((hdr_type & PCIM_MFDEV) == PCIM_MFDEV);
}
