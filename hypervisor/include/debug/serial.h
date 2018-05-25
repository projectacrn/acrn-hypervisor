/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SERIAL_H
#define SERIAL_H

#ifdef HV_DEBUG
int serial_init(void);
void uart16550_set_property(int enabled, int port_mapped, uint64_t base_addr);
#else
static inline int serial_init(void) { return 0; }
static inline void uart16550_set_property(__unused int enabled,
					__unused int port_mapped,
					__unused uint64_t base_addr)
{
}
#endif

#endif
