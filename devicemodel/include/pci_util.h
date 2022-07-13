/*
 * Copyright (C) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __PCI_UTIL_H
#define __PCI_UTIL_H

#include <stdbool.h>
#include "pciaccess.h"

int pci_find_cap(struct pci_device *pdev, const int cap_id);
int pci_find_ext_cap(struct pci_device *pdev, int cap_id);
int pci_get_pcie_type(struct pci_device *dev);
bool is_root_port(struct pci_device *pdev);
bool is_mfdev(struct pci_device *pdev);

#endif
