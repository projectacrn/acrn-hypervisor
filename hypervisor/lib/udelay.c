/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hv_lib.h>

void udelay(uint32_t us)
{
	uint64_t dest_tsc, delta_tsc;

	/* Calculate number of ticks to wait */
	delta_tsc = US_TO_TICKS(us);
	dest_tsc = rdtsc() + delta_tsc;

	/* Loop until time expired */
	while
		(rdtsc() < dest_tsc);
}
