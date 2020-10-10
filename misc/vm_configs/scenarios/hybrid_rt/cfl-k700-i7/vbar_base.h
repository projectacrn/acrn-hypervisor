/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VBAR_BASE_H_
#define VBAR_BASE_H_

#define VGA_COMPATIBLE_CONTROLLER_0_VBAR              .vbar_base[0] = 0xa0000000UL, \
                                                      .vbar_base[2] = 0x90000000UL

#define SYSTEM_PERIPHERAL_0_VBAR                      .vbar_base[0] = 0xa1938000UL

#define SIGNAL_PROCESSING_CONTROLLER_0_VBAR           .vbar_base[0] = 0xa1939000UL

#define USB_CONTROLLER_0_VBAR                         .vbar_base[0] = 0xa1920000UL

#define RAM_MEMORY_0_VBAR                             .vbar_base[0] = 0xa1934000UL, \
                                                      .vbar_base[2] = 0xa193a000UL

#define SERIAL_BUS_CONTROLLER_0_VBAR                  .vbar_base[0] = 0x8f800000UL

#define SERIAL_BUS_CONTROLLER_1_VBAR                  .vbar_base[0] = 0x8f801000UL

#define SERIAL_BUS_CONTROLLER_2_VBAR                  .vbar_base[0] = 0xfe010000UL

#define COMMUNICATION_CONTROLLER_0_VBAR               .vbar_base[0] = 0xa193d000UL

#define COMMUNICATION_CONTROLLER_1_VBAR               .vbar_base[0] = 0x8f802000UL

#define SERIAL_CONTROLLER_0_VBAR                      .vbar_base[1] = 0xa1943000UL

#define SATA_CONTROLLER_0_VBAR                        .vbar_base[0] = 0xa1936000UL, \
                                                      .vbar_base[1] = 0xa1942000UL, \
                                                      .vbar_base[5] = 0xa1941000UL

#define AUDIO_DEVICE_0_VBAR                           .vbar_base[0] = 0xa1930000UL, \
                                                      .vbar_base[4] = 0xa1000000UL

#define SMBUS_0_VBAR                                  .vbar_base[0] = 0xa193f000UL

#define ETHERNET_CONTROLLER_0_VBAR                    .vbar_base[0] = 0xa1900000UL

#define ETHERNET_CONTROLLER_1_VBAR                    .vbar_base[0] = 0xa1700000UL, \
                                                      .vbar_base[3] = 0xa1780000UL

#define ETHERNET_CONTROLLER_2_VBAR                    .vbar_base[0] = 0xa1600000UL, \
                                                      .vbar_base[3] = 0xa1680000UL

#define ETHERNET_CONTROLLER_3_VBAR                    .vbar_base[0] = 0xa1500000UL, \
                                                      .vbar_base[3] = 0xa1580000UL

#define ETHERNET_CONTROLLER_4_VBAR                    .vbar_base[0] = 0xa1400000UL, \
                                                      .vbar_base[3] = 0xa1480000UL

#define ETHERNET_CONTROLLER_5_VBAR                    .vbar_base[0] = 0xa1200000UL, \
                                                      .vbar_base[3] = 0xa1280000UL

#define ETHERNET_CONTROLLER_6_VBAR                    .vbar_base[0] = 0xa1100000UL, \
                                                      .vbar_base[3] = 0xa1180000UL

#define NON_VOLATILE_MEMORY_CONTROLLER_0_VBAR         .vbar_base[0] = 0xa1800000UL

#define NON_VOLATILE_MEMORY_CONTROLLER_1_VBAR         .vbar_base[0] = 0xa1300000UL

#define IVSHMEM_DEVICE_0_VBAR                         .vbar_base[0] = 0x80000000UL, \
                                                      .vbar_base[2] = 0x10000000cUL

#endif /* VBAR_BASE_H_ */
