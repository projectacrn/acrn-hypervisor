/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define HOST_BRIDGE		.pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U}

#define SATA_CONTROLLER_0	.pbdf.bits = {.b = 0x00U, .d = 0x17U, .f = 0x00U},	\
				.vbar_base[0] = 0xdf248000UL, 				\
				.vbar_base[1] = 0xdf24c000UL,				\
				.vbar_base[5] = 0xdf24b000UL

#define USB_CONTROLLER_0	.pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U}, 	\
				.vbar_base[0] = 0xdf230000UL

#define ETHERNET_CONTROLLER_0	.pbdf.bits = {.b = 0x00U, .d = 0x1fU, .f = 0x06U},	\
				.vbar_base[0] = 0xdf200000UL

#define NETWORK_CONTROLLER_0	.pbdf.bits = {.b = 0x01U, .d = 0x00U, .f = 0x00U},	\
				.vbar_base[0] = 0xdf100000UL

#endif /* PCI_DEVICES_H_ */
