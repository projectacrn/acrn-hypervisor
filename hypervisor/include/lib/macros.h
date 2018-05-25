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

/** Creates a bitfield mask.
 *
 * @param pos The position of the LSB within the mask.
 * @param width The width of the bitfield in bits.
 *
 * @return The bitfield mask.
 */

#define BITFIELD_MASK(pos, width) (((1<<(width))-1)<<(pos))
#define BITFIELD_VALUE(v, pos, width) (((v)<<(pos)) & (((1<<(width))-1)<<(pos)))

#define MAKE_BITFIELD_MASK(id) BITFIELD_MASK(id ## _POS, id ## _WIDTH)
#define MAKE_BITFIELD_VALUE(v, id) BITFIELD_VALUE(v, id ## _POS, id ## _WIDTH)

/** Defines a register within a register block. */
#define REGISTER(base, off) (base ## _BASE + (off))

#define MAKE_MMIO_REGISTER_ADDRESS(chip, module, register)	\
	(chip ## _ ## module ## _BASE +				\
	(chip ## _ ## module ## _ ## register ## _REGISTER))

/* Macro used to check if a value is aligned to the required boundary.
 * Returns TRUE if aligned; FALSE if not aligned
 * NOTE:  The required alignment must be a power of 2 (2, 4, 8, 16, 32, etc)
 */
#define MEM_ALIGNED_CHECK(value, req_align)                             \
	(((uint64_t)(value) & ((uint64_t)(req_align) - (uint64_t)1)) == 0)

#if !defined(ASSEMBLER) && !defined(LINKER_SCRIPT)

#define ARRAY_LENGTH(x) (sizeof(x)/sizeof((x)[0]))

#endif

#endif /* INCLUDE_MACROS_H defined */
