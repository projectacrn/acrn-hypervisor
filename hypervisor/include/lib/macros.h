/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MACROS_H
#define MACROS_H

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

#endif /* INCLUDE_MACROS_H defined */
