/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RELOCATE_H
#define RELOCATE_H

extern void relocate(void);
extern uint64_t get_hv_image_delta(void);
extern uint64_t get_hv_image_base(void);
extern uint64_t get_hv_image_size(void);

/* external symbols that are helpful for relocation */
extern uint8_t		_DYNAMIC[1];

#endif /* RELOCATE_H */
