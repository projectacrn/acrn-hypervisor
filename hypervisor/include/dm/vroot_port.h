/*
 * Copyright (c) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __VRP_H
#define __VRP_H

#include "vpci.h"

#define VRP_VENDOR			0x8086U
#define VRP_DEVICE			0x9d14U

extern const struct pci_vdev_ops vrp_ops;

int32_t create_vrp(struct acrn_vm *vm, struct acrn_vdev *dev);
int32_t destroy_vrp(struct pci_vdev *vdev);

#endif
