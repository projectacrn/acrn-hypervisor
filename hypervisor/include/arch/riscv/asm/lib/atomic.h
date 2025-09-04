/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef RISCV_ATOMIC_H
#define RISCV_ATOMIC_H

#include <types.h>

static inline uint64_t atomic_cmpxchg64(__unused volatile uint64_t *ptr, __unused uint64_t old, __unused uint64_t new)
{
	/**
	 * Dummy implementation.
	 * Official implementations are to be provided in the library patchset (by Haoyu).
	 */
	return 0UL;
}

#endif /* RISCV_ATOMIC_H */
