/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_X86_TSC_H
#define ARCH_X86_TSC_H

#include <types.h>

#define TSC_PER_MS	((uint64_t)get_tsc_khz())

/**
 * @brief Read Time Stamp Counter (TSC).
 *
 * @return TSC value
 */
static inline uint64_t rdtsc(void)
{
	uint32_t lo, hi;

	asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64_t)hi << 32U) | lo;
}

/**
 * @brief Get Time Stamp Counter (TSC) frequency in KHz.
 *
 * @return TSC frequency in KHz
 */
uint32_t get_tsc_khz(void);

/**
 * @brief Calibrate Time Stamp Counter (TSC) frequency.
 *
 * @return None
 */
void calibrate_tsc(void);

#endif	/* ARCH_X86_TSC_H */
