/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * BIOS Information
 * Vendor: Intel Corporation
 * Version: EHLSFWI1.R00.2224.A00.2005281500
 * Release Date: 05/28/2020
 *
 * Base Board Information
 * Manufacturer: Intel Corporation
 * Product Name: ElkhartLake LPDDR4x T3 CRB
 * Version: 2
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define HOST_BRIDGE                             .pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U}

#define VGA_COMPATIBLE_CONTROLLER_0             .pbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U}

#define SYSTEM_PERIPHERAL_0                     .pbdf.bits = {.b = 0x00U, .d = 0x08U, .f = 0x00U}

#define SYSTEM_PERIPHERAL_1                     .pbdf.bits = {.b = 0x00U, .d = 0x1DU, .f = 0x00U}

#define SERIAL_BUS_CONTROLLER_0                 .pbdf.bits = {.b = 0x00U, .d = 0x10U, .f = 0x00U}

#define SERIAL_BUS_CONTROLLER_1                 .pbdf.bits = {.b = 0x00U, .d = 0x10U, .f = 0x01U}

#define SERIAL_BUS_CONTROLLER_2                 .pbdf.bits = {.b = 0x00U, .d = 0x13U, .f = 0x00U}

#define SERIAL_BUS_CONTROLLER_3                 .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x00U}

#define SERIAL_BUS_CONTROLLER_4                 .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x02U}

#define SERIAL_BUS_CONTROLLER_5                 .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x03U}

#define SERIAL_BUS_CONTROLLER_6                 .pbdf.bits = {.b = 0x00U, .d = 0x19U, .f = 0x00U}

#define SERIAL_BUS_CONTROLLER_7                 .pbdf.bits = {.b = 0x00U, .d = 0x1BU, .f = 0x00U}

#define SERIAL_BUS_CONTROLLER_8                 .pbdf.bits = {.b = 0x00U, .d = 0x1BU, .f = 0x01U}

#define SERIAL_BUS_CONTROLLER_9                 .pbdf.bits = {.b = 0x00U, .d = 0x1BU, .f = 0x06U}

#define SERIAL_BUS_CONTROLLER_10                .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x05U}

#define COMMUNICATION_CONTROLLER_0              .pbdf.bits = {.b = 0x00U, .d = 0x13U, .f = 0x04U}

#define COMMUNICATION_CONTROLLER_1              .pbdf.bits = {.b = 0x00U, .d = 0x13U, .f = 0x05U}

#define COMMUNICATION_CONTROLLER_2              .pbdf.bits = {.b = 0x00U, .d = 0x16U, .f = 0x00U}

#define COMMUNICATION_CONTROLLER_3              .pbdf.bits = {.b = 0x00U, .d = 0x19U, .f = 0x02U}

#define COMMUNICATION_CONTROLLER_4              .pbdf.bits = {.b = 0x00U, .d = 0x1EU, .f = 0x00U}

#define COMMUNICATION_CONTROLLER_5              .pbdf.bits = {.b = 0x00U, .d = 0x1EU, .f = 0x01U}

#define USB_CONTROLLER_0                        .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U}

#define RAM_MEMORY_0                            .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x02U}

#define SATA_CONTROLLER_0                       .pbdf.bits = {.b = 0x00U, .d = 0x17U, .f = 0x00U}

#define SD_HOST_CONTROLLER_0                    .pbdf.bits = {.b = 0x00U, .d = 0x1AU, .f = 0x00U}

#define SD_HOST_CONTROLLER_1                    .pbdf.bits = {.b = 0x00U, .d = 0x1AU, .f = 0x01U}

#define NON_VGA_UNCLASSIFIED_DEVICE_0           .pbdf.bits = {.b = 0x00U, .d = 0x1AU, .f = 0x03U}

#define PCI_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x00U}

#define ETHERNET_CONTROLLER_0                   .pbdf.bits = {.b = 0x00U, .d = 0x1DU, .f = 0x01U}

#define ETHERNET_CONTROLLER_1                   .pbdf.bits = {.b = 0x00U, .d = 0x1DU, .f = 0x02U}

#define ETHERNET_CONTROLLER_2                   .pbdf.bits = {.b = 0x00U, .d = 0x1EU, .f = 0x04U}

#define ISA_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x00U}

#define MULTIMEDIA_AUDIO_CONTROLLER_0           .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x03U}

#define SMBUS_0                                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x04U}

#define NON_VOLATILE_MEMORY_CONTROLLER_0        .pbdf.bits = {.b = 0x01U, .d = 0x00U, .f = 0x00U}

#endif /* PCI_DEVICES_H_ */
