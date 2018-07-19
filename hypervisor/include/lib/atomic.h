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

#define	BUS_LOCK	"lock ; "

#define build_atomic_load(name, size, type, ptr)	\
static inline type name(const volatile type *ptr)	\
{							\
	type ret;					\
	asm volatile("mov" size " %1,%0"		\
			: "=r" (ret)			\
			: "m" (*ptr)			\
			: "cc", "memory");		\
	return ret;					\
}
build_atomic_load(atomic_load32, "l", uint32_t, p)
build_atomic_load(atomic_load64, "q", uint64_t, p)

#define build_atomic_store(name, size, type, ptr, v)	\
static inline void name(volatile type *ptr, type v)	\
{							\
	asm volatile("mov" size " %1,%0"		\
			: "=m" (*ptr)			\
			: "r" (v)			\
			: "cc", "memory");		\
}
build_atomic_store(atomic_store16, "w", uint16_t, p, v)
build_atomic_store(atomic_store32, "l", uint32_t, p, v)
build_atomic_store(atomic_store64, "q", uint64_t, p, v)

#define build_atomic_inc(name, size, type, ptr)		\
static inline void name(type *ptr)			\
{							\
	asm volatile(BUS_LOCK "inc" size " %0"		\
			: "=m" (*ptr)			\
			:  "m" (*ptr));			\
}
build_atomic_inc(atomic_inc32, "l", uint32_t, p)
build_atomic_inc(atomic_inc64, "q", uint64_t, p)

#define build_atomic_dec(name, size, type, ptr)		\
static inline void name(type *ptr)			\
{							\
	asm volatile(BUS_LOCK "dec" size " %0"		\
			: "=m" (*ptr)			\
			:  "m" (*ptr));			\
}
build_atomic_dec(atomic_dec16, "w", uint16_t, p)
build_atomic_dec(atomic_dec32, "l", uint32_t, p)
build_atomic_dec(atomic_dec64, "q", uint64_t, p)

/**
 *  #define atomic_set32(P, V)		(*(unsigned int *)(P) |= (V))
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
 *  uint32_t*   p       A pointer to memory area that stores source
 *                      value and setting result;
 *  uint32_t    v       The value needs to be set.
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
 *  uint32_t*   p       A pointer to memory area that stores source
 *                      value and clearing result;
 *  uint32_t    v       The value needs to be cleared.
 */
static inline void atomic_clear64(uint64_t *p, uint64_t v)
{
	__asm __volatile(BUS_LOCK "andq %1,%0"
			:  "+m" (*p)
			:  "r" (~v)
			:  "cc", "memory");
}

#define build_atomic_swap(name, size, type, ptr, v)	\
static inline type name(type *ptr, type v)		\
{							\
	asm volatile(BUS_LOCK "xchg" size " %1,%0"	\
			:  "+m" (*ptr), "+r" (v)	\
			:				\
			:  "cc", "memory");		\
	return v;					\
}
build_atomic_swap(atomic_swap32, "l", uint32_t, p, v)
build_atomic_swap(atomic_swap64, "q", uint64_t, p, v)

 /*
 * #define atomic_readandclear32(P) \
 * (return (*(uint32_t *)(P)); *(uint32_t *)(P) = 0U;)
  */
#define atomic_readandclear32(p)		atomic_swap32(p, 0U)

 /*
 * #define atomic_readandclear64(P) \
 * (return (*(uint64_t *)(P)); *(uint64_t *)(P) = 0UL;)
  */
#define atomic_readandclear64(p)	atomic_swap64(p, 0UL)

#define build_atomic_cmpxchg(name, size, type, ptr, old, new)	\
static inline type name(volatile type *ptr,			\
			type old, type new)			\
{								\
	type ret;						\
	asm volatile(BUS_LOCK "cmpxchg" size " %2,%1"		\
			: "=a" (ret), "+m" (*p)			\
			: "r" (new), "0" (old)			\
			: "memory");				\
	return ret;						\
}
build_atomic_cmpxchg(atomic_cmpxchg32, "l", uint32_t, p, old, new)
build_atomic_cmpxchg(atomic_cmpxchg64, "q", uint64_t, p, old, new)

#define build_atomic_xadd(name, size, type, ptr, v)		\
static inline type name(type *ptr, type v)			\
{								\
	asm volatile(BUS_LOCK "xadd" size " %0,%1"		\
			: "+r" (v), "+m" (*p)			\
			:					\
			: "cc", "memory");			\
	return v;						\
 }
build_atomic_xadd(atomic_xadd16, "w", uint16_t, p, v)
build_atomic_xadd(atomic_xadd32, "l", int, p, v)
build_atomic_xadd(atomic_xadd64, "q", long, p, v)

#define atomic_add_return(p, v)		( atomic_xadd32(p, v) + v )
#define atomic_sub_return(p, v)		( atomic_xadd32(p, -v) - v )

#define atomic_inc_return(v)		atomic_add_return((v), 1)
#define atomic_dec_return(v)		atomic_sub_return((v), 1)

#define atomic_add64_return(p, v)	( atomic_xadd64(p, v) + v )
#define atomic_sub64_return(p, v)	( atomic_xadd64(p, -v) - v )

#define atomic_inc64_return(v)		atomic_add64_return((v), 1)
#define atomic_dec64_return(v)		atomic_sub64_return((v), 1)

#endif /* ATOMIC_H*/
