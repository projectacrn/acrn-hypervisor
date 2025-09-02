/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/ticks.h>

/* arch_cpu_ticks() and arch_cpu_tickrate() are provided in arch specific modules */

uint64_t us_to_ticks(uint32_t us)
{
	return (((uint64_t)us * (uint64_t)arch_cpu_tickrate()) / 1000UL);
}

uint64_t ticks_to_us(uint64_t ticks)
{
	uint64_t us = 0UL;
	uint64_t khz = arch_cpu_tickrate();

	if (khz != 0U) {
		us = (ticks * 1000UL) / (uint64_t)khz;
	}

	return us;
}

uint64_t ticks_to_ms(uint64_t ticks)
{
	return ticks / (uint64_t)arch_cpu_tickrate();
}

uint64_t cpu_ticks(void)
{
	return arch_cpu_ticks();
}
