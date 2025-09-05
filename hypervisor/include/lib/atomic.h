/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ATOMIC_H
#define ATOMIC_H
#include <types.h>
#include <asm/lib/atomic.h>

/* The mandatory functions should be implemented by arch atomic library */
static inline void arch_atomic_inc32(uint32_t * ptr);
static inline void arch_atomic_inc64(uint64_t * ptr);
static inline void arch_atomic_dec32(uint32_t * ptr);
static inline void arch_atomic_dec64(uint64_t * ptr);
static inline uint32_t arch_atomic_cmpxchg32(volatile uint32_t * ptr, uint32_t old, uint32_t new);
static inline uint64_t arch_atomic_cmpxchg64(volatile uint64_t * ptr, uint64_t old, uint64_t new);
static inline uint32_t arch_atomic_swap32(uint32_t *p, uint32_t v);
static inline uint64_t arch_atomic_swap64(uint64_t *p, uint64_t v);
static inline int32_t arch_atomic_add_return(int32_t *p, int32_t v);
static inline int32_t arch_atomic_sub_return(int32_t *p, int32_t v);
static inline int32_t arch_atomic_inc_return(int32_t *v);
static inline int32_t arch_atomic_dec_return(int32_t *v);
static inline int64_t arch_atomic_add64_return(int64_t *p, int64_t v);
static inline int64_t arch_atomic_sub64_return(int64_t *p, int64_t v);
static inline int64_t arch_atomic_inc64_return(int64_t *v);
static inline int64_t arch_atomic_dec64_return(int64_t *v);

/* The common functions map to arch implementation */
static inline void atomic_inc32(uint32_t * ptr)
{
	return arch_atomic_inc32(ptr);
}

static inline void atomic_inc64(uint64_t * ptr)
{
	return arch_atomic_inc64(ptr);
}

static inline void atomic_dec32(uint32_t * ptr)
{
	return arch_atomic_dec32(ptr);
}

static inline void atomic_dec64(uint64_t * ptr)
{
	return arch_atomic_dec64(ptr);
}

static inline uint32_t atomic_cmpxchg32(volatile uint32_t * ptr, uint32_t old, uint32_t new)
{
	return arch_atomic_cmpxchg32(ptr, old, new);
}

static inline uint64_t atomic_cmpxchg64(volatile uint64_t * ptr, uint64_t old, uint64_t new)
{
	return arch_atomic_cmpxchg64(ptr, old, new);
}

static inline uint32_t atomic_readandclear32(uint32_t *p)
{
	return arch_atomic_swap32(p, 0U);
}

static inline uint64_t atomic_readandclear64(uint64_t *p)
{
	return arch_atomic_swap64(p, 0UL);
}

static inline int32_t atomic_add_return(int32_t *p, int32_t v)
{
	return arch_atomic_add_return(p, v);
}

static inline int32_t atomic_sub_return(int32_t *p, int32_t v)
{
	return arch_atomic_sub_return(p, v);
}

static inline int32_t atomic_inc_return(int32_t *v)
{
	return arch_atomic_inc_return(v);
}

static inline int32_t atomic_dec_return(int32_t *v)
{
	return arch_atomic_dec_return(v);
}

static inline int64_t atomic_add64_return(int64_t *p, int64_t v)
{
	return arch_atomic_add64_return(p, v);
}

static inline int64_t atomic_sub64_return(int64_t *p, int64_t v)
{
	return arch_atomic_sub64_return(p, v);
}

static inline int64_t atomic_inc64_return(int64_t *v)
{
	return arch_atomic_inc64_return(v);
}

static inline int64_t atomic_dec64_return(int64_t *v)
{
	return arch_atomic_dec64_return(v);
}

#endif /* ATOMIC_H*/