/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * BIOS Information
 * Vendor: INSYDE Corp.
 * Version: Z01-0001A027
 * Release Date: 10/14/2019
 * BIOS Revision: 1.28
 *
 * Base Board Information
 * Manufacturer: Logic Supply
 * Product Name: RXM-181
 * Version: Type2 - Board Version
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define HOST_BRIDGE                             .pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U}

#define VGA_COMPATIBLE_CONTROLLER_0             .pbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U}

#define SYSTEM_PERIPHERAL_0                     .pbdf.bits = {.b = 0x00U, .d = 0x08U, .f = 0x00U}

#define SIGNAL_PROCESSING_CONTROLLER_0          .pbdf.bits = {.b = 0x00U, .d = 0x12U, .f = 0x00U}

#define USB_CONTROLLER_0                        .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U}

#define RAM_MEMORY_0                            .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x02U}

#define SERIAL_BUS_CONTROLLER_0                 .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x00U}

#define SERIAL_BUS_CONTROLLER_1                 .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x01U}

#define SERIAL_BUS_CONTROLLER_2                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x05U}

#define COMMUNICATION_CONTROLLER_0              .pbdf.bits = {.b = 0x00U, .d = 0x16U, .f = 0x00U}

#define COMMUNICATION_CONTROLLER_1              .pbdf.bits = {.b = 0x00U, .d = 0x1EU, .f = 0x00U}

#define SERIAL_CONTROLLER_0                     .pbdf.bits = {.b = 0x00U, .d = 0x16U, .f = 0x03U}

#define SATA_CONTROLLER_0                       .pbdf.bits = {.b = 0x00U, .d = 0x17U, .f = 0x00U}

#define PCI_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1BU, .f = 0x00U}

#define PCI_BRIDGE_1                            .pbdf.bits = {.b = 0x00U, .d = 0x1BU, .f = 0x06U}

#define PCI_BRIDGE_2                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x00U}

#define PCI_BRIDGE_3                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x06U}

#define PCI_BRIDGE_4                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x07U}

#define PCI_BRIDGE_5                            .pbdf.bits = {.b = 0x02U, .d = 0x00U, .f = 0x00U}

#define PCI_BRIDGE_6                            .pbdf.bits = {.b = 0x03U, .d = 0x01U, .f = 0x00U}

#define PCI_BRIDGE_7                            .pbdf.bits = {.b = 0x03U, .d = 0x02U, .f = 0x00U}

#define PCI_BRIDGE_8                            .pbdf.bits = {.b = 0x03U, .d = 0x03U, .f = 0x00U}

#define PCI_BRIDGE_9                            .pbdf.bits = {.b = 0x03U, .d = 0x04U, .f = 0x00U}

#define PCI_BRIDGE_10                           .pbdf.bits = {.b = 0x03U, .d = 0x05U, .f = 0x00U}

#define ISA_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x00U}

#define AUDIO_DEVICE_0                          .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x03U}

#define SMBUS_0                                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x04U}

#define ETHERNET_CONTROLLER_0                   .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x06U}

#define ETHERNET_CONTROLLER_1                   .pbdf.bits = {.b = 0x04U, .d = 0x00U, .f = 0x00U}

#define ETHERNET_CONTROLLER_2                   .pbdf.bits = {.b = 0x05U, .d = 0x00U, .f = 0x00U}

#define ETHERNET_CONTROLLER_3                   .pbdf.bits = {.b = 0x06U, .d = 0x00U, .f = 0x00U}

#define ETHERNET_CONTROLLER_4                   .pbdf.bits = {.b = 0x07U, .d = 0x00U, .f = 0x00U}

#define ETHERNET_CONTROLLER_5                   .pbdf.bits = {.b = 0x0AU, .d = 0x00U, .f = 0x00U}

#define ETHERNET_CONTROLLER_6                   .pbdf.bits = {.b = 0x0BU, .d = 0x00U, .f = 0x00U}

#define NON_VOLATILE_MEMORY_CONTROLLER_0        .pbdf.bits = {.b = 0x01U, .d = 0x00U, .f = 0x00U}

#define NON_VOLATILE_MEMORY_CONTROLLER_1        .pbdf.bits = {.b = 0x09U, .d = 0x00U, .f = 0x00U}

#endif /* PCI_DEVICES_H_ */
