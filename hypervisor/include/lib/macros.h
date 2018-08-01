/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MACROS_H
#define MACROS_H

/** Replaces 'x' by the string "x". */
#define __CPP_STRING(x) #x
/** Replaces 'x' by its value. */
#define CPP_STRING(x) __CPP_STRING(x)

/* Macro used to check if a value is aligned to the required boundary.
 * Returns TRUE if aligned; FALSE if not aligned
 * NOTE:  The required alignment must be a power of 2 (2, 4, 8, 16, 32, etc)
 */
#define MEM_ALIGNED_CHECK(value, req_align)                             \
	(((uint64_t)(value) & ((uint64_t)(req_align) - 1UL)) == 0UL)

#if !defined(ASSEMBLER) && !defined(LINKER_SCRIPT)

#define ARRAY_LENGTH(x) (sizeof(x)/sizeof((x)[0]))

#endif

#endif /* INCLUDE_MACROS_H defined */
