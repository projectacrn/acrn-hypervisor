/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VBAR_BASE_H_
#define VBAR_BASE_H_

#define VGA_COMPATIBLE_CONTROLLER_0_VBAR                                                                               \
	.vbar_base[0] = HI_MMIO_START + 0x0UL, .vbar_base[2] = HI_MMIO_START + 0x10000000UL

#define SYSTEM_PERIPHERAL_0_VBAR .vbar_base[0] = HI_MMIO_START + 0x20000000UL

#define USB_CONTROLLER_0_VBAR .vbar_base[0] = HI_MMIO_START + 0x20010000UL

#define USB_CONTROLLER_1_VBAR .vbar_base[0] = HI_MMIO_START + 0x20040000UL, .vbar_base[2] = HI_MMIO_START + 0x20080000UL

#define USB_CONTROLLER_2_VBAR .vbar_base[0] = HI_MMIO_START + 0x200c0000UL, .vbar_base[2] = HI_MMIO_START + 0x20100000UL

#define USB_CONTROLLER_3_VBAR .vbar_base[0] = HI_MMIO_START + 0x20110000UL

#define RAM_MEMORY_0_VBAR .vbar_base[0] = HI_MMIO_START + 0x20120000UL, .vbar_base[2] = HI_MMIO_START + 0x20124000UL

#define NETWORK_CONTROLLER_0_VBAR .vbar_base[0] = HI_MMIO_START + 0x20128000UL

#define SERIAL_BUS_CONTROLLER_0_VBAR .vbar_base[0] = HI_MMIO_START + 0x2012c000UL

#define SERIAL_BUS_CONTROLLER_1_VBAR .vbar_base[0] = HI_MMIO_START + 0x2012d000UL

#define SERIAL_BUS_CONTROLLER_2_VBAR .vbar_base[0] = HI_MMIO_START + 0x20301000UL

#define COMMUNICATION_CONTROLLER_0_VBAR .vbar_base[0] = HI_MMIO_START + 0x2012e000UL

#define SATA_CONTROLLER_0_VBAR                                                                                         \
	.vbar_base[0] = HI_MMIO_START + 0x20130000UL, .vbar_base[1] = HI_MMIO_START + 0x20132000UL,                    \
	.vbar_base[5] = HI_MMIO_START + 0x20132800UL

#define AUDIO_DEVICE_0_VBAR .vbar_base[0] = HI_MMIO_START + 0x20134000UL, .vbar_base[4] = HI_MMIO_START + 0x20200000UL

#define SMBUS_0_VBAR .vbar_base[0] = HI_MMIO_START + 0x20300000UL

#define NON_VOLATILE_MEMORY_CONTROLLER_0_VBAR .vbar_base[0] = HI_MMIO_START + 0x20304000UL

#define ETHERNET_CONTROLLER_0_VBAR                                                                                     \
	.vbar_base[0] = HI_MMIO_START + 0x20400000UL, .vbar_base[3] = HI_MMIO_START + 0x20500000UL

#endif /* VBAR_BASE_H_ */
