/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define PTDEV_HI_MMIO_SIZE	0xe00000UL

#define SATA_CONTROLLER_0	.pbdf.bits = {.b = 0x00U, .d = 0x17U, .f = 0x00U}

#define USB_CONTROLLER_0	.pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U}

#define ETHERNET_CONTROLLER_0	.pbdf.bits = {.b = 0x00U, .d = 0x1fU, .f = 0x06U}

#define NETWORK_CONTROLLER_0	.pbdf.bits = {.b = 0x01U, .d = 0x00U, .f = 0x00U}

#endif /* PCI_DEVICES_H_ */
