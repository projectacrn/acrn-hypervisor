/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define PTDEV_HI_MMIO_SIZE	0xe00000UL

#define SATA_CONTROLLER_0	.pbdf.bits = {.b = 0x00U, .d = 0x17U, .f = 0x00U},	\
				.vbar_base[0] = PTDEV_HI_MMIO_START + 0x200000UL, 	\
				.vbar_base[1] = PTDEV_HI_MMIO_START + 0x400000UL,	\
				.vbar_base[5] = PTDEV_HI_MMIO_START + 0x600000UL

#define USB_CONTROLLER_0	.pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U}, 	\
				.vbar_base[0] = PTDEV_HI_MMIO_START + 0x800000UL

#define ETHERNET_CONTROLLER_0	.pbdf.bits = {.b = 0x00U, .d = 0x1fU, .f = 0x06U},	\
				.vbar_base[0] = PTDEV_HI_MMIO_START + 0xa00000UL

#define NETWORK_CONTROLLER_0	.pbdf.bits = {.b = 0x01U, .d = 0x00U, .f = 0x00U},	\
				.vbar_base[0] = PTDEV_HI_MMIO_START + 0xc00000UL

#endif /* PCI_DEVICES_H_ */
