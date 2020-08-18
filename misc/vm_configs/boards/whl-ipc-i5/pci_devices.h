/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
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

#define HOST_BRIDGE                             .pbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U}

#define VGA_COMPATIBLE_CONTROLLER_0             .pbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U}

#define SIGNAL_PROCESSING_CONTROLLER_0          .pbdf.bits = {.b = 0x00U, .d = 0x12U, .f = 0x00U}

#define USB_CONTROLLER_0                        .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x00U}

#define RAM_MEMORY_0                            .pbdf.bits = {.b = 0x00U, .d = 0x14U, .f = 0x02U}

#define COMMUNICATION_CONTROLLER_0              .pbdf.bits = {.b = 0x00U, .d = 0x16U, .f = 0x00U}

#define SATA_CONTROLLER_0                       .pbdf.bits = {.b = 0x00U, .d = 0x17U, .f = 0x00U}

#define SD_HOST_CONTROLLER_0                    .pbdf.bits = {.b = 0x00U, .d = 0x1AU, .f = 0x00U}

#define PCI_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x00U}

#define PCI_BRIDGE_1                            .pbdf.bits = {.b = 0x00U, .d = 0x1CU, .f = 0x04U}

#define PCI_BRIDGE_2                            .pbdf.bits = {.b = 0x00U, .d = 0x1DU, .f = 0x00U}

#define PCI_BRIDGE_3                            .pbdf.bits = {.b = 0x00U, .d = 0x1DU, .f = 0x01U}

#define ISA_BRIDGE_0                            .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x00U}

#define AUDIO_DEVICE_0                          .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x03U}

#define SMBUS_0                                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x04U}

#define SERIAL_BUS_CONTROLLER_0                 .pbdf.bits = {.b = 0x00U, .d = 0x1FU, .f = 0x05U}

#define NON_VOLATILE_MEMORY_CONTROLLER_0        .pbdf.bits = {.b = 0x02U, .d = 0x00U, .f = 0x00U}

#define ETHERNET_CONTROLLER_0                   .pbdf.bits = {.b = 0x03U, .d = 0x00U, .f = 0x00U}

#define ETHERNET_CONTROLLER_1                   .pbdf.bits = {.b = 0x04U, .d = 0x00U, .f = 0x00U}

#define IVSHMEM_SHM_REGION_0                    "hv:/shm_region_0"

#endif /* PCI_DEVICES_H_ */
