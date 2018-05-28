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

#ifndef TYPES_H
#define TYPES_H

/* Define NULL value */
#define		HV_NULL			0

/* Defines for TRUE / FALSE conditions */
#define		HV_FALSE		0
#define		HV_TRUE			1

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define __aligned(x)		__attribute__((aligned(x)))
#define __packed	__attribute__((packed))
#define	__unused	__attribute__((unused))

#ifndef ASSEMBLER

/* Define standard data types.  These definitions allow software components
 * to perform in the same manner on different target platforms.
 */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef signed long int64_t;
typedef unsigned int size_t;
typedef __builtin_va_list va_list;

typedef uint8_t bool;

#ifndef NULL
#define         NULL                                ((void *) 0)
#endif

#ifndef true
#define true		1
#define false		0
#endif

#ifndef UINT64_MAX
#define UINT64_MAX	(-1UL)
#endif

#endif /* ASSEMBLER */

#endif /* INCLUDE_TYPES_H defined */
