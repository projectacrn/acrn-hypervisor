/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
