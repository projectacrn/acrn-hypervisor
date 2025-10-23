/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>

/**
 * @brief Generate a random 64-bit value using RISC-V timing counters
 *
 * This function provides a portable random number generation mechanism for
 * RISC-V systems by combining entropy from hardware timing counters. Since
 * the Zkr (Entropy Source) extension is optional in RVA23 profile and not
 * universally available, this implementation uses standard timing counters
 * that are present in all RISC-V systems.
 *
 * The implementation combines two entropy sources:
 * - rdcycle: CPU cycle counter (high frequency, fast changing)
 * - rdtime: Real-time counter (lower frequency, slower changing)
 *
 * The time counter value is left-shifted by 13 bits before XORing with the
 * cycle counter. The shift value 13 is chosen to compensate for the frequency
 * difference between counters.
 *
 * @return uint64_t A 64-bit pseudo-random value
 *
 * @fixme TODO: Detect Zkr extension availability and use CSR_SEED (0x015)
 *        when hardware entropy source is present for better randomness quality.
 */
uint64_t arch_get_random_value(void)
{
	uint64_t cycle, time;

	asm volatile ("rdcycle %0" : "=r"(cycle));
	asm volatile ("rdtime %0" : "=r"(time));

	return cycle ^ (time << 13U);
}
