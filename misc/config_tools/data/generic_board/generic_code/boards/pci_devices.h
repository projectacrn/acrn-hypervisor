/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * BIOS Information
 * Vendor: Intel Corp.
 * Version: TNTGL357.0042.2020.1221.1743
 * Release Date: 12/21/2020
 * BIOS Revision: 5.19
 *
 * Base Board Information
 * Manufacturer: Intel Corporation
 * Product Name: NUC11TNBi5
 * Version: M11904-403
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define HOST_BRIDGE                             .pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U}

#define VGA_COMPATIBLE_CONTROLLER_0             .pbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U}

#define PCI_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x06U, .f = 0x00U}

#define PCI_BRIDGE_1                            .pbdf.bits = {.b = 0x00U, .d = 0x07U, .f = 0x00U}

#define PCI_BRIDGE_2                            .pbdf.bits = {.b = 0x00U, .d = 0x07U, .f = 0x02U}

#define PCI_BRIDGE_3                            .pbdf.bits = {.b = 0x00U, .d = 0x1DU, .f = 0x00U}

#define SYSTEM_PERIPHERAL_0                     .pbdf.bits = {.b = 0x00U, .d = 0x08U, .f = 0x00U}

#define USB_CONTROLLER_0                        .pbdf.bits = {.b = 0x00U, .d = 0x0DU, .f = 0x00U}

#define USB_CONTROLLER_1                        .pbdf.bits = {.b = 0x00U, .d = 0x0DU, .f = 0x02U}

#define USB_CONTROLLER_2                        .pbdf.bits = {.b = 0x00U, .d = 0x0DU, .f = 0x03U}

#define USB_CONTROLLER_3                        .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U}

#define RAM_MEMORY_0                            .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x02U}

#define NETWORK_CONTROLLER_0                    .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x03U}

#define SERIAL_BUS_CONTROLLER_0                 .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x00U}

#define SERIAL_BUS_CONTROLLER_1                 .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x01U}

#define SERIAL_BUS_CONTROLLER_2                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x05U}

#define COMMUNICATION_CONTROLLER_0              .pbdf.bits = {.b = 0x00U, .d = 0x16U, .f = 0x00U}

#define SATA_CONTROLLER_0                       .pbdf.bits = {.b = 0x00U, .d = 0x17U, .f = 0x00U}

#define ISA_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x00U}

#define AUDIO_DEVICE_0                          .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x03U}

#define SMBUS_0                                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x04U}

#define NON_VOLATILE_MEMORY_CONTROLLER_0        .pbdf.bits = {.b = 0x01U, .d = 0x00U, .f = 0x00U}

#define ETHERNET_CONTROLLER_0                   .pbdf.bits = {.b = 0x58U, .d = 0x00U, .f = 0x00U}

#endif /* PCI_DEVICES_H_ */
