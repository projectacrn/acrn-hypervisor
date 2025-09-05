/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haoyu Tang <haoyu.tang@intel.com>
 */

#ifndef BITS_H
#define BITS_H
#include <types.h>
#include <asm/lib/bits.h>

/**
 *
 * INVALID_BIT_INDEX means when input paramter is zero,
 * bit operations function can't find bit set and return
 * the invalid bit index directly.
 *
 **/
#ifndef INVALID_BIT_INDEX
#define INVALID_BIT_INDEX  0xffffU
#endif

/* The mandatory functions should be implemented by arch bits library */
static inline int16_t arch_fls32(uint32_t value);
static inline uint16_t arch_fls64(uint64_t value);
static inline uint16_t arch_ffs64(uint64_t value);
static inline void arch_bitmap_set(uint32_t nr_arg, volatile uint64_t *addr);
static inline void arch_bitmap32_set(uint32_t nr_arg, volatile uint32_t *addr);
static inline void arch_bitmap_clear(uint32_t nr_arg, volatile uint64_t *addr);
static inline void arch_bitmap32_clear(uint32_t nr_arg, volatile uint32_t *addr);
static inline bool arch_bitmap_test_and_set(uint32_t nr_arg, volatile uint64_t *addr);
static inline bool arch_bitmap32_test_and_set(uint32_t nr_arg, volatile uint32_t *addr);
static inline bool arch_bitmap_test_and_clear(uint32_t nr_arg, volatile uint64_t *addr);
static inline bool arch_bitmap32_test_and_clear(uint32_t nr_arg, volatile uint32_t *addr);
static inline bool arch_bitmap_test(uint32_t nr, const volatile uint64_t *addr);
static inline bool arch_bitmap32_test(uint32_t nr, const volatile uint32_t *addr);

/* The common functions map to arch implementation */
static inline int16_t fls32(uint32_t value)
{
	return arch_fls32(value);
}

static inline uint16_t fls64(uint64_t value)
{
	return arch_fls64(value);
}

static inline uint16_t ffs64(uint64_t value)
{
	return arch_ffs64(value);
}

static inline uint16_t ffz64(uint64_t value)
{
	return arch_ffs64(~value);
}

static inline void bitmap_set(uint32_t nr_arg, volatile uint64_t *addr)
{
	arch_bitmap_set(nr_arg, addr);
}

static inline void bitmap32_set(uint32_t nr_arg, volatile uint32_t *addr)
{
	arch_bitmap32_set(nr_arg, addr);
}

static inline void bitmap_clear(uint32_t nr_arg, volatile uint64_t *addr)
{
	arch_bitmap_clear(nr_arg, addr);
}

static inline void bitmap32_clear(uint32_t nr_arg, volatile uint32_t *addr)
{
	arch_bitmap32_clear(nr_arg, addr);
}

static inline bool bitmap_test_and_set(uint32_t nr_arg, volatile uint64_t *addr)
{
	return arch_bitmap_test_and_set(nr_arg, addr);
}

static inline bool bitmap32_test_and_set(uint32_t nr_arg, volatile uint32_t *addr)
{
	return arch_bitmap32_test_and_set(nr_arg, addr);
}

static inline bool bitmap_test_and_clear(uint32_t nr_arg, volatile uint64_t *addr)
{
	return arch_bitmap_test_and_clear(nr_arg, addr);
}

static inline bool bitmap32_test_and_clear(uint32_t nr_arg, volatile uint32_t *addr)
{
	return arch_bitmap32_test_and_clear(nr_arg, addr);
}

static inline bool bitmap_test(uint32_t nr, const volatile uint64_t *addr)
{
	return arch_bitmap_test(nr, addr);
}

static inline bool bitmap32_test(uint32_t nr, const volatile uint32_t *addr)
{
	return arch_bitmap32_test(nr, addr);
}

/* The funcitons are implemented in common bits library */
uint64_t ffz64_ex(const uint64_t *addr, uint64_t size);
uint16_t clz32(uint32_t value);
uint16_t clz64(uint64_t value);
uint16_t bitmap_weight(uint64_t bits);
void bitmap_set_non_atomic(uint32_t nr_arg, volatile uint64_t *addr);
void bitmap32_set_non_atomic(uint32_t nr_arg, volatile uint32_t *addr);
void bitmap_clear_non_atomic(uint32_t nr_arg, volatile uint64_t *addr);
void bitmap32_clear_non_atomic(uint32_t nr_arg, volatile uint32_t *addr);
bool bitmap_test_and_set_non_atomic(uint32_t nr_arg, volatile uint64_t *addr);
bool bitmap32_test_and_set_non_atomic(uint32_t nr_arg, volatile uint32_t *addr);
bool bitmap_test_and_clear_non_atomic(uint32_t nr_arg, volatile uint64_t *addr);
bool bitmap32_test_and_clear_non_atomic(uint32_t nr_arg, volatile uint32_t *addr);

#endif /* BITS_H*/
