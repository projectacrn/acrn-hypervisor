/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haoyu Tang <haoyu.tang@intel.com>
 */

#include <bits.h>

/* common functions implementation */
uint64_t ffz64_ex(const uint64_t *addr, uint64_t size)
{
	uint64_t ret = size;
	uint64_t idx;

	for (idx = 0UL; (idx << 6U) < size; idx++) {
		if (addr[idx] != ~0UL) {
			ret = (idx << 6U) + ffz64(addr[idx]);
			break;
		}
	}

	return ret;
}

uint16_t clz32(uint32_t value)
{
	return ((value != 0U) ? (31U - fls32(value)) : 32U);
}

uint16_t clz64(uint64_t value)
{
	return ((value != 0UL) ? (63U - fls64(value)) : 64U);
}

void bitmap_set_non_atomic(uint32_t nr_arg, volatile uint64_t *addr)
{
	uint64_t mask = 1UL << (nr_arg & ((8U * sizeof(uint64_t)) - 1U));
	*addr |= mask;
}

void bitmap32_set_non_atomic(uint32_t nr_arg, volatile uint32_t *addr)
{
	uint32_t mask = 1U << (nr_arg & ((8U * sizeof(uint32_t)) - 1U));
	*addr |= mask;
}

void bitmap_clear_non_atomic(uint32_t nr_arg, volatile uint64_t *addr)
 {
	uint64_t mask = 1UL << (nr_arg & ((8U * sizeof(uint64_t)) - 1U));
	*addr &= ~mask;
}

void bitmap32_clear_non_atomic(uint32_t nr_arg, volatile uint32_t *addr)
{
	uint32_t mask = 1U << (nr_arg & ((8U * sizeof(uint32_t)) - 1U));
	*addr &= ~mask;
}

bool bitmap_test_and_set_non_atomic(uint32_t nr_arg, volatile uint64_t *addr)
{
	uint64_t mask = 1UL << (nr_arg & ((8U * sizeof(uint64_t)) - 1U));
	bool old = !!(*addr & mask);
	*addr |= mask;
	return old;
}

bool bitmap32_test_and_set_non_atomic(uint32_t nr_arg, volatile uint32_t *addr)
{
	uint32_t mask = 1U << (nr_arg & ((8U * sizeof(uint32_t)) - 1U));
	bool old = !!(*addr & mask);
	*addr |= mask;
	return old;
}

bool bitmap_test_and_clear_non_atomic(uint32_t nr_arg, volatile uint64_t *addr)
{
	uint64_t mask = 1UL << (nr_arg & ((8U * sizeof(uint64_t)) - 1U));
	bool old = !!(*addr & mask);
	*addr &= ~mask;
	return old;
}

bool bitmap32_test_and_clear_non_atomic(uint32_t nr_arg, volatile uint32_t *addr)
{
	uint32_t mask = 1U << (nr_arg & ((8U * sizeof(uint32_t)) - 1U));
	bool old = !!(*addr & mask);
	*addr &= ~mask;
	return old;
}

uint16_t bitmap_weight(uint64_t bits)
{
	return (uint16_t)__builtin_popcountl(bits);
}
