/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __PCI_UTIL_H
#define __PCI_UTIL_H

#include <stdbool.h>
#include "pciaccess.h"

struct pci_device_info {
	bool is_bridge;
	int primary_bus;
	int secondary_bus;
	int subordinate_bus;
	uint16_t bdf;
	struct pci_device_info *parent;  /* pointer to its parent bridge */
	struct pci_device_info *clist;  /* children list */

	/* cache of all pci devices
	 * FIXME: remove PCI_DEVICE_Q.  To cleanup pci device cache:
	 * remove children, then remove parents
	 */
	TAILQ_ENTRY(pci_device_info) PCI_DEVICE_Q;
};

int pci_find_cap(struct pci_device *pdev, const int cap_id);
int pci_find_ext_cap(struct pci_device *pdev, int cap_id);
int pci_get_pcie_type(struct pci_device *dev);
bool is_root_port(struct pci_device *pdev);
bool is_bridge(struct pci_device *pdev);

int get_device_count_on_bus(int bus);
int get_device_count_on_bridge(const struct pci_device_info *bridge_info);
int scan_pci(void);
struct pci_device * pci_find_root_port(const struct pci_device *pdev);
void clean_pci_cache(void);

#endif
