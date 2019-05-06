/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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

#define min(x, y)	((x) < (y)) ? (x) : (y)

#define max(x, y)	((x) < (y)) ? (y) : (x)

/** Replaces 'x' by the string "x". */
#define STRINGIFY(x) #x

/* Macro used to check if a value is aligned to the required boundary.
 * Returns TRUE if aligned; FALSE if not aligned
 * NOTE:  The required alignment must be a power of 2 (2, 4, 8, 16, 32, etc)
 */
static inline bool mem_aligned_check(uint64_t value, uint64_t req_align)
{
	return ((value & (req_align - 1UL)) == 0UL);
}

#endif /* UTIL_H */
