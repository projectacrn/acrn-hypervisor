/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hv_lib.h>

void mdelay(uint32_t loop_count)
{
	/* Loop until done */
	while (loop_count != 0U) {
		/* Delay for 1 ms */
		udelay(1000U);
		loop_count--;
	}
}
