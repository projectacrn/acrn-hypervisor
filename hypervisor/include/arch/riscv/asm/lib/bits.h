/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef RISCV_BITS_H
#define RISCV_BITS_H

#include <types.h>

uint16_t ffs64(__unused uint64_t value)
{
	/**
	 * Dummy implementation.
	 * Official implementations are to be provided in the library patchset (by Haoyu).
	 */
	return 0U;
}

bool bitmap_test(__unused uint16_t nr, __unused const volatile uint64_t *addr)
{
	/**
	 * Dummy implementation.
	 * Official implementations are to be provided in the library patchset (by Haoyu).
	 */
	return true;
}

void bitmap_clear_lock(__unused uint16_t nr_arg, __unused volatile uint64_t *addr)
{
	/**
	 * Dummy implementation.
	 * Official implementations are to be provided in the library patchset (by Haoyu).
	 */
}

void bitmap_clear_nolock(__unused uint16_t nr_arg, __unused volatile uint64_t *addr)
{
	/**
	 * Dummy implementation.
	 * Official implementations are to be provided in the library patchset (by Haoyu).
	 */
}

#endif /* RISCV_BITS_H */
