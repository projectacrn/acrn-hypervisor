/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HW_TIMER_H
#define HW_TIMER_H

#include <types.h>

void set_hw_timeout(uint64_t timeout);
void init_hw_timer(void);

#endif
