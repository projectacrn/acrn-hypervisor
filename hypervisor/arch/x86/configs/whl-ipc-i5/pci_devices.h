/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * BIOS Information
 * Vendor: American Megatrends Inc.
 * Version: WL10R104
 * Release Date: 09/12/2019
 * BIOS Revision: 5.13
 *
 * Base Board Information
 * Manufacturer: Maxtang
 * Product Name: WL10
 * Version: V1.0
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define PTDEV_HI_MMIO_SIZE			0UL

#define VGA_COMPATIBLE_CONTROLLER_0             .pbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa0000000UL, \
                                                .vbar_base[2] = 0x90000000UL
#define SIGNAL_PROCESSING_CONTROLLER_0          .pbdf.bits = {.b = 0x00U, .d = 0x12U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa141e000UL
#define USB_CONTROLLER_0                        .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1400000UL
#define RAM_MEMORY_0                            .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x02U}, \
                                                .vbar_base[0] = 0xa1416000UL, \
                                                .vbar_base[2] = 0xa141d000UL
#define COMMUNICATION_CONTROLLER_0              .pbdf.bits = {.b = 0x00U, .d = 0x16U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa141c000UL
#define SATA_CONTROLLER_0                       .pbdf.bits = {.b = 0x00U, .d = 0x17U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1414000UL, \
                                                .vbar_base[1] = 0xa141b000UL, \
                                                .vbar_base[5] = 0xa141a000UL
#define SD_HOST_CONTROLLER_0                    .pbdf.bits = {.b = 0x00U, .d = 0x1AU, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1419000UL
#define PCI_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x00U}
#define PCI_BRIDGE_1                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x04U}
#define PCI_BRIDGE_2                            .pbdf.bits = {.b = 0x00U, .d = 0x1DU, .f = 0x00U}
#define PCI_BRIDGE_3                            .pbdf.bits = {.b = 0x00U, .d = 0x1DU, .f = 0x01U}
#define ISA_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x00U}
#define AUDIO_DEVICE_0                          .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x03U}, \
                                                .vbar_base[0] = 0xa1410000UL, \
                                                .vbar_base[4] = 0xa1000000UL
#define SMBUS_0                                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x04U}, \
                                                .vbar_base[0] = 0xa1418000UL
#define SERIAL_BUS_CONTROLLER_0                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x05U}, \
                                                .vbar_base[0] = 0xfe010000UL
#define NON_VOLATILE_MEMORY_CONTROLLER_0        .pbdf.bits = {.b = 0x02U, .d = 0x00U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1300000UL
#define ETHERNET_CONTROLLER_0                   .pbdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1200000UL, \
                                                .vbar_base[3] = 0xa1220000UL
#define ETHERNET_CONTROLLER_1                   .pbdf.bits = {.b = 0x04U, .d = 0x00U, .f = 0x00U}, \
                                                .vbar_base[0] = 0xa1100000UL, \
                                                .vbar_base[3] = 0xa1120000UL

#endif /* PCI_DEVICES_H_ */
