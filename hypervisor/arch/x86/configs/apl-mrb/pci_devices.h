/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define HOST_BRIDGE		.pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U}

#define SATA_CONTROLLER_0	.pbdf.bits = {.b = 0x00U, .d = 0x12U, .f = 0x00U},	\
				.vbar_base[0] = 0xb3f10000UL,				\
				.vbar_base[1] = 0xb3f53000UL,				\
				.vbar_base[5] = 0xb3f52000UL

#define USB_CONTROLLER_0	.pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x00U},	\
				.vbar_base[0] = 0xb3f00000UL

#define ETHERNET_CONTROLLER_0	.pbdf.bits = {.b = 0x02U, .d = 0x00U, .f = 0x00U},	\
				.vbar_base[0] = 0xb3c00000UL,				\
				.vbar_base[3] = 0xb3c80000UL

#endif /* PCI_DEVICES_H_ */
