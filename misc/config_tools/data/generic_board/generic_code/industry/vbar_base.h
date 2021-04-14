/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VBAR_BASE_H_
#define VBAR_BASE_H_

#define VGA_COMPATIBLE_CONTROLLER_0_VBAR .vbar_base[0] = 0x82000000UL, .vbar_base[2] = HI_MMIO_START + 0x0UL

#define SYSTEM_PERIPHERAL_0_VBAR .vbar_base[0] = 0x834e4000UL

#define SYSTEM_PERIPHERAL_1_VBAR .vbar_base[0] = 0x83000000UL

#define SERIAL_BUS_CONTROLLER_0_VBAR .vbar_base[0] = 0x83441000UL

#define SERIAL_BUS_CONTROLLER_1_VBAR .vbar_base[0] = 0x83444000UL

#define SERIAL_BUS_CONTROLLER_2_VBAR .vbar_base[0] = 0x834d8000UL

#define SERIAL_BUS_CONTROLLER_3_VBAR .vbar_base[0] = 0x83445000UL

#define SERIAL_BUS_CONTROLLER_4_VBAR .vbar_base[0] = 0x83446000UL

#define SERIAL_BUS_CONTROLLER_5_VBAR .vbar_base[0] = 0x83447000UL

#define SERIAL_BUS_CONTROLLER_6_VBAR .vbar_base[0] = 0x83448000UL

#define SERIAL_BUS_CONTROLLER_7_VBAR .vbar_base[0] = 0x834da000UL

#define SERIAL_BUS_CONTROLLER_8_VBAR .vbar_base[0] = 0x834dc000UL

#define SERIAL_BUS_CONTROLLER_9_VBAR .vbar_base[0] = 0x834de000UL

#define SERIAL_BUS_CONTROLLER_10_VBAR .vbar_base[0] = 0x8344c000UL, .vbar_base[1] = 0x80000000UL

#define COMMUNICATION_CONTROLLER_0_VBAR .vbar_base[0] = 0x84600000UL

#define COMMUNICATION_CONTROLLER_1_VBAR .vbar_base[0] = 0x845fc000UL

#define COMMUNICATION_CONTROLLER_2_VBAR .vbar_base[0] = 0x834eb000UL

#define COMMUNICATION_CONTROLLER_3_VBAR .vbar_base[0] = 0x83449000UL

#define COMMUNICATION_CONTROLLER_4_VBAR .vbar_base[0] = 0x8344a000UL

#define COMMUNICATION_CONTROLLER_5_VBAR .vbar_base[0] = 0x8344b000UL

#define USB_CONTROLLER_0_VBAR .vbar_base[0] = 0x834c0000UL

#define RAM_MEMORY_0_VBAR .vbar_base[0] = 0x834d0000UL, .vbar_base[2] = 0x834e7000UL

#define SATA_CONTROLLER_0_VBAR .vbar_base[0] = 0x834e2000UL, .vbar_base[1] = 0x834f6000UL, .vbar_base[5] = 0x834f5000UL

#define SD_HOST_CONTROLLER_0_VBAR .vbar_base[0] = 0x834ee000UL

#define SD_HOST_CONTROLLER_1_VBAR .vbar_base[0] = 0x834ef000UL

#define NON_VGA_UNCLASSIFIED_DEVICE_0_VBAR .vbar_base[0] = 0x83400000UL

#define ETHERNET_CONTROLLER_0_VBAR .vbar_base[0] = 0x83500000UL

#define ETHERNET_CONTROLLER_1_VBAR .vbar_base[0] = 0x83480000UL

#define ETHERNET_CONTROLLER_2_VBAR .vbar_base[0] = 0x83442000UL, .vbar_base[2] = 0x834f2000UL

#define MULTIMEDIA_AUDIO_CONTROLLER_0_VBAR .vbar_base[0] = 0x834d4000UL, .vbar_base[4] = 0x83200000UL

#define SMBUS_0_VBAR .vbar_base[0] = 0x834f3000UL

#define NON_VOLATILE_MEMORY_CONTROLLER_0_VBAR .vbar_base[0] = 0x83300000UL

#endif /* VBAR_BASE_H_ */
