/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef BOARD_H
#define BOARD_H

#include <types.h>

struct platform_clos_info {
	uint32_t clos_mask;
	uint32_t msr_index;
};

extern struct platform_clos_info platform_clos_array[];
extern uint16_t platform_clos_num;

#endif /* BOARD_H */
