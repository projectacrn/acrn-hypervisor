/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define HOST_BRIDGE		.pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U}
#define SATA_CONTROLLER		.pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U}
#define USB_CONTROLLER		.pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x00U}

#define STORAGE_CONTROLLER_0	SATA_CONTROLLER
#define STORAGE_CONTROLLER_1	USB_CONTROLLER

#define ETHERNET_CONTROLLER_0	.pbdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x00U}
#define ETHERNET_CONTROLLER_1	.pbdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x01U}

#endif /* PCI_DEVICES_H_ */
