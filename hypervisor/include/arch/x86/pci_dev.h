/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PCI_DEV_H_
#define PCI_DEV_H_

#include <vpci.h>

struct acrn_vm_config;

void fill_pci_dev_config(struct pci_pdev *pdev);
void initialize_sos_pci_dev_config(struct acrn_vm_config *vm_config);

#endif /* PCI_DEV_H_ */
