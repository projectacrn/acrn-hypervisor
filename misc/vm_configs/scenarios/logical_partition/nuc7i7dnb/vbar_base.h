/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VBAR_BASE_H_
#define VBAR_BASE_H_

#define VGA_COMPATIBLE_CONTROLLER_0_VBAR              .vbar_base[0] = 0xde000000UL, \
                                                      .vbar_base[2] = 0xc0000000UL

#define SYSTEM_PERIPHERAL_0_VBAR                      .vbar_base[0] = 0xdf252000UL

#define USB_CONTROLLER_0_VBAR                         .vbar_base[0] = 0xdf230000UL

#define SIGNAL_PROCESSING_CONTROLLER_0_VBAR           .vbar_base[0] = 0xdf251000UL

#define SIGNAL_PROCESSING_CONTROLLER_1_VBAR           .vbar_base[0] = 0xdf250000UL

#define SIGNAL_PROCESSING_CONTROLLER_2_VBAR           .vbar_base[0] = 0xdf24f000UL

#define COMMUNICATION_CONTROLLER_0_VBAR               .vbar_base[0] = 0xdf24e000UL

#define SERIAL_CONTROLLER_0_VBAR                      .vbar_base[1] = 0xdf24d000UL

#define SATA_CONTROLLER_0_VBAR                        .vbar_base[0] = 0xdf248000UL, \
                                                      .vbar_base[1] = 0xdf24c000UL, \
                                                      .vbar_base[5] = 0xdf24b000UL

#define MEMORY_CONTROLLER_0_VBAR                      .vbar_base[0] = 0xdf244000UL

#define AUDIO_DEVICE_0_VBAR                           .vbar_base[0] = 0xdf240000UL, \
                                                      .vbar_base[4] = 0xdf220000UL

#define SMBUS_0_VBAR                                  .vbar_base[0] = 0xdf24a000UL

#define ETHERNET_CONTROLLER_0_VBAR                    .vbar_base[0] = 0xdf200000UL

#define NETWORK_CONTROLLER_0_VBAR                     .vbar_base[0] = 0xdf100000UL

#define NON_VOLATILE_MEMORY_CONTROLLER_0_VBAR         .vbar_base[0] = 0xdf000000UL

#endif /* VBAR_BASE_H_ */
