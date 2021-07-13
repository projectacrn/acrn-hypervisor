/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMCS9900_H
#define VMCS9900_H

#define MCS9900_VENDOR		0x9710U
#define MCS9900_DEV		0x9900U

extern const struct pci_vdev_ops vmcs9900_ops;
void trigger_vmcs9900_msix(struct pci_vdev *vdev);
int32_t create_vmcs9900_vdev(struct acrn_vm *vm, struct acrn_vdev *dev);
int32_t destroy_vmcs9900_vdev(struct pci_vdev *vdev);

#endif
