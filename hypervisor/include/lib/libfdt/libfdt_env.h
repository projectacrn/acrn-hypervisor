/* SPDX-License-Identifier: GPL-2.0-or-later OR BSD-2-Clause */
#ifndef LIBFDT_ENV_H
#define LIBFDT_ENV_H
/*
 * libfdt - Flat Device Tree manipulation
 * Copyright (C) 2006 David Gibson, IBM Corporation.
 * Copyright 2012 Kim Phillips, Freescale Semiconductor.
 */

#include <io.h>
#include <rtl.h>
#include <memory.h>

#define INT_MAX		((int)(~0U >> 1))
#define UINT_MAX	((unsigned int)~0U)
#define INT32_MAX	INT_MAX

#ifdef __CHECKER__
#define FDT_FORCE __attribute__((force))
#define FDT_BITWISE __attribute__((bitwise))
#else
#define FDT_FORCE
#define FDT_BITWISE
#endif

typedef uint16_t be16_t;
typedef uint32_t be32_t;
typedef uint64_t be64_t;

typedef be16_t FDT_BITWISE fdt16_t;
typedef be32_t FDT_BITWISE fdt32_t;
typedef be64_t FDT_BITWISE fdt64_t;

static inline uint16_t fdt16_to_cpu(fdt16_t x)
{
	return (FDT_FORCE uint16_t)be16_to_cpu(x);
}

static inline fdt16_t cpu_to_fdt16(uint16_t x)
{
	return (FDT_FORCE fdt16_t)cpu_to_be16(x);
}

static inline uint32_t fdt32_to_cpu(fdt32_t x)
{
	return (FDT_FORCE uint32_t)be32_to_cpu(x);
}

static inline fdt32_t cpu_to_fdt32(uint32_t x)
{
	return (FDT_FORCE fdt32_t)cpu_to_be32(x);
}

static inline uint64_t fdt64_to_cpu(fdt64_t x)
{
	return (FDT_FORCE uint64_t)be64_to_cpu(x);
}

static inline fdt64_t cpu_to_fdt64(uint64_t x)
{
	return (FDT_FORCE fdt64_t)cpu_to_be64(x);
}

#endif /* LIBFDT_ENV_H */
