/*
 * Copyright (c) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __VROOT_PORT_H
#define __VROOT_PORT_H

#include "vpci.h"

#define VROOT_PORT_VENDOR			0x8086U
#define VROOT_PORT_DEVICE			0x9d14U

extern const struct pci_vdev_ops vroot_port_ops;

#endif
