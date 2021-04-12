/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/ticks.h>
#include <common/delay.h>

void udelay(uint32_t us)
{
	uint64_t end, delta;

	/* Calculate number of ticks to wait */
	delta = us_to_ticks(us);
	end = cpu_ticks() + delta;

	/* Loop until time expired */
	while (cpu_ticks() < end) {
	}
}
