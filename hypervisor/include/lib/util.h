/*
 * Copyright (C) 2018-2020 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UTIL_H
#define UTIL_H
#include <types.h>

#define offsetof(st, m) __builtin_offsetof(st, m)
#define va_start	__builtin_va_start
#define va_end		__builtin_va_end

/** Roundup (x/y) to ( x/y + (x%y) ? 1 : 0) **/
#define INT_DIV_ROUNDUP(x, y)	((((x)+(y))-1)/(y))

/** Roundup (x) to (y) aligned **/
#define roundup(x, y)  (((x) + ((y) - 1UL)) & (~((y) - 1UL)))

#define min(x, y)	(((x) < (y)) ? (x) : (y))

#define max(x, y)	(((x) < (y)) ? (y) : (x))

/** Replaces 'x' by the string "x". */
#define STRINGIFY(x) #x

/*
 * This algorithm get the round up 2^n
 *
 * n = input - 1;    0x1002 ---->  0x1001
 * n |= n >> 1;      0x1001 | 0x800
 * n |= n >> 2;      0x1801 | 0x600
 * n |= n >> 4;      0x1e01 | 0x1e0
 * n |= n >> 8;      0x1fe1 | 0x1f
 * n |= n >> 16;     0x1fff
 * n |= n >> 32;     0x1fff
 * n += 1;           0x2000
 */
#define ROUND0(x)  ((x)-1)
#define ROUND1(x)  (ROUND0(x) |(ROUND0(x)>>1))
#define ROUND2(x)  (ROUND1(x) |(ROUND1(x)>>2))
#define ROUND4(x)  (ROUND2(x) |(ROUND2(x)>>4))
#define ROUND8(x)  (ROUND4(x) |(ROUND4(x)>>8))
#define ROUND16(x) (ROUND8(x) |(ROUND8(x)>>16))
#define ROUND32(x) (ROUND16(x) |(ROUND16(x)>>32))
#define powerof2_roundup(x) ((ROUND32(x) == (~0UL)) ? ~0UL : (ROUND32(x) +1))

/* Macro used to check if a value is aligned to the required boundary.
 * Returns TRUE if aligned; FALSE if not aligned
 * NOTE:  The required alignment must be a power of 2 (2, 4, 8, 16, 32, etc)
 */
static inline bool mem_aligned_check(uint64_t value, uint64_t req_align)
{
	return ((value & (req_align - 1UL)) == 0UL);
}

/**
 * @pre buf != NULL
 */
static inline uint8_t calculate_sum8(const void *buf, uint32_t length)
{
	uint32_t i;
	uint8_t sum = 0U;

	for (i = 0U; i < length; i++) {
		sum += *((const uint8_t *)buf + i);
	}

	return sum;
}

/**
 * @pre buf != NULL
 */
static inline uint8_t calculate_checksum8(const void *buf, uint32_t len)
{
	return (uint8_t)(0x100U - calculate_sum8(buf, len));
}

/**
 * @pre (uuid1 != NULL) && (uuid2 != NULL)
 */
static inline bool uuid_is_equal(const uint8_t *uuid1, const uint8_t *uuid2)
{
	uint64_t uuid1_h = *(const uint64_t *)uuid1;
	uint64_t uuid1_l = *(const uint64_t *)(uuid1 + 8);
	uint64_t uuid2_h = *(const uint64_t *)uuid2;
	uint64_t uuid2_l = *(const uint64_t *)(uuid2 + 8);

	return ((uuid1_h == uuid2_h) && (uuid1_l == uuid2_l));
}

#endif /* UTIL_H */
