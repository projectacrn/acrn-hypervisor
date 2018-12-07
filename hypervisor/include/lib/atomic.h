/*-
 * Copyright (c) 1998 Doug Rabson
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef ATOMIC_H
#define ATOMIC_H
#include <cpu.h>

#define build_atomic_load(name, size, type)		\
static inline type name(const volatile type *ptr)	\
{							\
	type ret;					\
	asm volatile("mov" size " %1,%0"		\
			: "=r" (ret)			\
			: "m" (*ptr)			\
			: "cc", "memory");		\
	return ret;					\
}
build_atomic_load(atomic_load16, "w", uint16_t)
build_atomic_load(atomic_load32, "l", uint32_t)
build_atomic_load(atomic_load64, "q", uint64_t)

#define build_atomic_store(name, size, type)		\
static inline void name(volatile type *ptr, type v)	\
{							\
	asm volatile("mov" size " %1,%0"		\
			: "=m" (*ptr)			\
			: "r" (v)			\
			: "cc", "memory");		\
}
build_atomic_store(atomic_store16, "w", uint16_t)
build_atomic_store(atomic_store32, "l", uint32_t)
build_atomic_store(atomic_store64, "q", uint64_t)

#define build_atomic_inc(name, size, type)		\
static inline void name(type *ptr)			\
{							\
	asm volatile(BUS_LOCK "inc" size " %0"		\
			: "=m" (*ptr)			\
			:  "m" (*ptr));			\
}
build_atomic_inc(atomic_inc16, "w", uint16_t)
build_atomic_inc(atomic_inc32, "l", uint32_t)
build_atomic_inc(atomic_inc64, "q", uint64_t)

#define build_atomic_dec(name, size, type)		\
static inline void name(type *ptr)			\
{							\
	asm volatile(BUS_LOCK "dec" size " %0"		\
			: "=m" (*ptr)			\
			:  "m" (*ptr));			\
}
build_atomic_dec(atomic_dec16, "w", uint16_t)
build_atomic_dec(atomic_dec32, "l", uint32_t)
build_atomic_dec(atomic_dec64, "q", uint64_t)

/**
 *  #define atomic_set32(P, V)		(*(uint32_t *)(P) |= (V))
 * 
 *  Parameters:
 *  uint32_t*	p	A pointer to memory area that stores source
 *			value and setting result;
 *  uint32_t	v	The value needs to be set.
 */
static inline void atomic_set32(uint32_t *p, uint32_t v)
{
	__asm __volatile(BUS_LOCK "orl %1,%0"
			:  "+m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_clear32(P, V)		(*(uint32_t *)(P) &= ~(V))
 *  Parameters:
 *  uint32_t*	p	A pointer to memory area that stores source
 *			value and clearing result;
 *  uint32_t	v	The value needs to be cleared.
 */
static inline void atomic_clear32(uint32_t *p, uint32_t v)
{
	__asm __volatile(BUS_LOCK "andl %1,%0"
			:  "+m" (*p)
			:  "r" (~v)
			:  "cc", "memory");
}

/*
 *  #define atomic_set64(P, V)		(*(uint64_t *)(P) |= (V))
 *
 *  Parameters:
 *  uint64_t*   p       A pointer to memory area that stores source
 *                      value and setting result;
 *  uint64_t    v       The value needs to be set.
 */
static inline void atomic_set64(uint64_t *p, uint64_t v)
{
	__asm __volatile(BUS_LOCK "orq %1,%0"
			:  "+m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_clear64(P, V)	(*(uint64_t *)(P) &= ~(V))
 *
 *  Parameters:
 *  uint64_t*   p       A pointer to memory area that stores source
 *                      value and clearing result;
 *  uint64_t    v       The value needs to be cleared.
 */
static inline void atomic_clear64(uint64_t *p, uint64_t v)
{
	__asm __volatile(BUS_LOCK "andq %1,%0"
			:  "+m" (*p)
			:  "r" (~v)
			:  "cc", "memory");
}

#define build_atomic_swap(name, size, type)		\
static inline type name(type *ptr, type v)		\
{							\
	asm volatile(BUS_LOCK "xchg" size " %1,%0"	\
			:  "+m" (*ptr), "+r" (v)	\
			:				\
			:  "cc", "memory");		\
	return v;					\
}
build_atomic_swap(atomic_swap32, "l", uint32_t)
build_atomic_swap(atomic_swap64, "q", uint64_t)

 /*
 * #define atomic_readandclear32(P) \
 * (return (*(uint32_t *)(P)); *(uint32_t *)(P) = 0U;)
  */
static inline uint32_t atomic_readandclear32(uint32_t *p)
{
	return atomic_swap32(p, 0U);
}

 /*
 * #define atomic_readandclear64(P) \
 * (return (*(uint64_t *)(P)); *(uint64_t *)(P) = 0UL;)
  */
static inline uint64_t atomic_readandclear64(uint64_t *p)
{
	return atomic_swap64(p, 0UL);
}

#define build_atomic_cmpxchg(name, size, type)			\
static inline type name(volatile type *ptr, type old, type new)	\
{								\
	type ret;						\
	asm volatile(BUS_LOCK "cmpxchg" size " %2,%1"		\
			: "=a" (ret), "+m" (*ptr)		\
			: "r" (new), "0" (old)			\
			: "memory");				\
	return ret;						\
}
build_atomic_cmpxchg(atomic_cmpxchg32, "l", uint32_t)
build_atomic_cmpxchg(atomic_cmpxchg64, "q", uint64_t)

#define build_atomic_xadd(name, size, type)			\
static inline type name(type *ptr, type v)			\
{								\
	asm volatile(BUS_LOCK "xadd" size " %0,%1"		\
			: "+r" (v), "+m" (*ptr)			\
			:					\
			: "cc", "memory");			\
	return v;						\
 }
build_atomic_xadd(atomic_xadd16, "w", uint16_t)
build_atomic_xadd(atomic_xadd32, "l", int32_t)
build_atomic_xadd(atomic_xadd64, "q", int64_t)

static inline int32_t atomic_add_return(int32_t *p, int32_t v)
{
	return (atomic_xadd32(p, v) + v);
}

static inline int32_t atomic_sub_return(int32_t *p, int32_t v)
{
	return (atomic_xadd32(p, -v) - v);
}

static inline int32_t atomic_inc_return(int32_t *v)
{
	return atomic_add_return(v, 1);
}

static inline int32_t atomic_dec_return(int32_t *v)
{
	return atomic_sub_return(v, 1);
}

static inline int64_t atomic_add64_return(int64_t *p, int64_t v)
{
	return (atomic_xadd64(p, v) + v);
}

static inline int64_t atomic_sub64_return(int64_t *p, int64_t v)
{
	return (atomic_xadd64(p, -v) - v);
}

static inline int64_t atomic_inc64_return(int64_t *v)
{
	return atomic_add64_return(v, 1);
}

static inline int64_t atomic_dec64_return(int64_t *v)
{
	return atomic_sub64_return(v, 1);
}

#endif /* ATOMIC_H*/
