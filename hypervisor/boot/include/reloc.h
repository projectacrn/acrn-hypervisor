/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RELOCATE_H
#define RELOCATE_H

extern void _relocate(void);
extern uint64_t get_hv_image_delta(void);
extern uint64_t get_hv_image_base(void);
extern uint64_t trampoline_relo_addr(void *addr);

/* external symbols that are helpful for relocation */
extern uint8_t         _DYNAMIC[];

extern uint8_t         cpu_primary_start_32[];
extern uint8_t         cpu_primary_start_64[];

extern uint8_t         trampoline_spinlock_ptr[];

#endif /* RELOCATE_H */
