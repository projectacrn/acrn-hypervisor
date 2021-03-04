/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PCI_DEV_H_
#define PCI_DEV_H_

#include <x86/vm_config.h>

extern struct acrn_vm_pci_dev_config sos_pci_devs[CONFIG_MAX_PCI_DEV_NUM];

struct pci_pdev;
struct acrn_vm_pci_dev_config *init_one_dev_config(struct pci_pdev *pdev);

#endif /* PCI_DEV_H_ */
