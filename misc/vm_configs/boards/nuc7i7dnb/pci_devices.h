/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * BIOS Information
 * Vendor: Intel Corp.
 * Version: DNKBLi7v.86A.0065.2019.0611.1424
 * Release Date: 06/11/2019
 * BIOS Revision: 5.6
 *
 * Base Board Information
 * Manufacturer: Intel Corporation
 * Product Name: NUC7i7DNB
 * Version: J83500-204
 */

#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_

#define HOST_BRIDGE                             .pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U}

#define VGA_COMPATIBLE_CONTROLLER_0             .pbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U}

#define SYSTEM_PERIPHERAL_0                     .pbdf.bits = {.b = 0x00U, .d = 0x08U, .f = 0x00U}

#define USB_CONTROLLER_0                        .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U}

#define SIGNAL_PROCESSING_CONTROLLER_0          .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x02U}

#define SIGNAL_PROCESSING_CONTROLLER_1          .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x00U}

#define SIGNAL_PROCESSING_CONTROLLER_2          .pbdf.bits = {.b = 0x00U, .d = 0x15U, .f = 0x01U}

#define COMMUNICATION_CONTROLLER_0              .pbdf.bits = {.b = 0x00U, .d = 0x16U, .f = 0x00U}

#define SERIAL_CONTROLLER_0                     .pbdf.bits = {.b = 0x00U, .d = 0x16U, .f = 0x03U}

#define SATA_CONTROLLER_0                       .pbdf.bits = {.b = 0x00U, .d = 0x17U, .f = 0x00U}

#define PCI_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x00U}

#define PCI_BRIDGE_1                            .pbdf.bits = {.b = 0x00U, .d = 0x1DU, .f = 0x00U}

#define ISA_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x00U}

#define MEMORY_CONTROLLER_0                     .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x02U}

#define AUDIO_DEVICE_0                          .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x03U}

#define SMBUS_0                                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x04U}

#define ETHERNET_CONTROLLER_0                   .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x06U}

#define NETWORK_CONTROLLER_0                    .pbdf.bits = {.b = 0x01U, .d = 0x00U, .f = 0x00U}

#define NON_VOLATILE_MEMORY_CONTROLLER_0        .pbdf.bits = {.b = 0x02U, .d = 0x00U, .f = 0x00U}

#endif /* PCI_DEVICES_H_ */
