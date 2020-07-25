/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VBAR_BASE_H_
#define VBAR_BASE_H_

#define VGA_COMPATIBLE_CONTROLLER_0_VBAR              .vbar_base[0] = 0xa0000000UL, \
                                                      .vbar_base[2] = 0x90000000UL

#define SIGNAL_PROCESSING_CONTROLLER_0_VBAR           .vbar_base[0] = 0xa141e000UL

#define USB_CONTROLLER_0_VBAR                         .vbar_base[0] = 0xa1400000UL

#define RAM_MEMORY_0_VBAR                             .vbar_base[0] = 0xa1416000UL, \
                                                      .vbar_base[2] = 0xa141d000UL

#define COMMUNICATION_CONTROLLER_0_VBAR               .vbar_base[0] = 0xa141c000UL

#define SATA_CONTROLLER_0_VBAR                        .vbar_base[0] = 0xa1414000UL, \
                                                      .vbar_base[1] = 0xa141b000UL, \
                                                      .vbar_base[5] = 0xa141a000UL

#define SD_HOST_CONTROLLER_0_VBAR                     .vbar_base[0] = 0xa1419000UL

#define AUDIO_DEVICE_0_VBAR                           .vbar_base[0] = 0xa1410000UL, \
                                                      .vbar_base[4] = 0xa1000000UL

#define SMBUS_0_VBAR                                  .vbar_base[0] = 0xa1418000UL

#define SERIAL_BUS_CONTROLLER_0_VBAR                  .vbar_base[0] = 0xfe010000UL

#define NON_VOLATILE_MEMORY_CONTROLLER_0_VBAR         .vbar_base[0] = 0xa1300000UL

#define ETHERNET_CONTROLLER_0_VBAR                    .vbar_base[0] = 0xa1200000UL, \
                                                      .vbar_base[3] = 0xa1220000UL

#define ETHERNET_CONTROLLER_1_VBAR                    .vbar_base[0] = 0xa1100000UL, \
                                                      .vbar_base[3] = 0xa1120000UL

#endif /* VBAR_BASE_H_ */
