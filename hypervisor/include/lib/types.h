/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
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
