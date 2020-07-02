/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * BIOS Information
 * Vendor: American Megatrends Inc.
 * Version: WL37R107
 * Release Date: 06/24/2020
 * BIOS Revision: 5.13
 *
 * Base Board Information
 * Manufacturer: Maxtang
 * Product Name: WL37
 * Version: V1.0
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define PTDEV_HI_MMIO_SIZE                      0x0UL

#define ETHERNET_CONTROLLER_0                   .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x06U}, \
                                                .vbar_base[0] = 0xa1400000UL

#define ETHERNET_CONTROLLER_1                   .pbdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1200000UL, \
                                                .vbar_base[3] = 0xa1220000UL

#define ETHERNET_CONTROLLER_2                   .pbdf.bits = {.b = 0x02U, .d = 0x00U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1300000UL, \
                                                .vbar_base[3] = 0xa1320000UL

#define COMMUNICATION_CONTROLLER_0              .pbdf.bits = {.b = 0x00U, .d = 0x19U, .f = 0x02U}, \
                                                .vbar_base[0] = 0xa143d000UL

#define COMMUNICATION_CONTROLLER_1              .pbdf.bits = {.b = 0x00U, .d = 0x1EU, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1443000UL

#define COMMUNICATION_CONTROLLER_2              .pbdf.bits = {.b = 0x00U, .d = 0x1EU, .f = 0x01U}, \
                                                .vbar_base[0] = 0xa1444000UL

#define COMMUNICATION_CONTROLLER_3              .pbdf.bits = {.b = 0x00U, .d = 0x16U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1441000UL

#define NON_VOLATILE_MEMORY_CONTROLLER_0        .pbdf.bits = {.b = 0x05U, .d = 0x00U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1100000UL

#define SMBUS_0                                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x04U}, \
                                                .vbar_base[0] = 0xa1438000UL

#define PCI_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1DU, .f = 0x04U}

#define PCI_BRIDGE_1                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x06U}

#define PCI_BRIDGE_2                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x07U}

#define PCI_BRIDGE_3                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x00U}

#define PCI_BRIDGE_4                            .pbdf.bits = {.b = 0x00U, .d = 0x1DU, .f = 0x00U}

#define RAM_MEMORY_0                            .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x02U}, \
                                                .vbar_base[0] = 0xa1436000UL, \
                                                .vbar_base[2] = 0xa1446000UL

#define SERIAL_BUS_CONTROLLER_0                 .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa143a000UL

#define SERIAL_BUS_CONTROLLER_1                 .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x02U}, \
                                                .vbar_base[0] = 0xa143c000UL

#define SERIAL_BUS_CONTROLLER_2                 .pbdf.bits = {.b = 0x00U, .d = 0x19U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1442000UL

#define SERIAL_BUS_CONTROLLER_3                 .pbdf.bits = {.b = 0x00U, .d = 0x1EU, .f = 0x03U}, \
                                                .vbar_base[0] = 0xa1447000UL

#define SERIAL_BUS_CONTROLLER_4                 .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x01U}, \
                                                .vbar_base[0] = 0xa143b000UL

#define SERIAL_BUS_CONTROLLER_5                 .pbdf.bits = {.b = 0x00U, .d = 0x1EU, .f = 0x02U}, \
                                                .vbar_base[0] = 0xa1445000UL

#define SERIAL_BUS_CONTROLLER_6                 .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x03U}, \
                                                .vbar_base[0] = 0xa143e000UL

#define SERIAL_BUS_CONTROLLER_7                 .pbdf.bits = {.b = 0x00U, .d = 0x12U, .f = 0x06U}, \
                                                .vbar_base[0] = 0xa1439000UL

#define SERIAL_BUS_CONTROLLER_8                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x05U}, \
                                                .vbar_base[0] = 0xfe010000UL

#define ISA_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x00U}

#define HOST_BRIDGE                             .pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U}

#define AUDIO_DEVICE_0                          .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x03U}, \
                                                .vbar_base[0] = 0xa1430000UL, \
                                                .vbar_base[4] = 0xa1000000UL

#define SATA_CONTROLLER_0                       .pbdf.bits = {.b = 0x00U, .d = 0x17U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1434000UL, \
                                                .vbar_base[1] = 0xa1440000UL, \
                                                .vbar_base[5] = 0xa143f000UL

#define VGA_COMPATIBLE_CONTROLLER_0             .pbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa0000000UL, \
                                                .vbar_base[2] = 0x90000000UL

#define SIGNAL_PROCESSING_CONTROLLER_0          .pbdf.bits = {.b = 0x00U, .d = 0x12U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1448000UL

#define USB_CONTROLLER_0                        .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1420000UL

#endif /* PCI_DEVICES_H_ */
