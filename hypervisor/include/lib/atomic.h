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
build_atomic_load(atomic_load, "l", int, p)
build_atomic_load(atomic_load64, "q", long, p)

#define build_atomic_store(name, size, type, ptr, v)	\
static inline void name(volatile type *ptr, type v)	\
{							\
	asm volatile("mov" size " %1,%0"		\
			: "=m" (*ptr)			\
			: "r" (v)			\
			: "cc", "memory");		\
}
build_atomic_store(atomic_store, "l", int, p, v)
build_atomic_store(atomic_store64, "q", long, p, v)

#define build_atomic_inc(name, size, type, ptr)		\
static inline void name(type *ptr)			\
{							\
	asm volatile(BUS_LOCK "inc" size " %0"		\
			: "=m" (*ptr)			\
			:  "m" (*ptr));			\
}
build_atomic_inc(atomic_inc, "l", int, p)
build_atomic_inc(atomic_inc64, "q", long, p)

#define build_atomic_dec(name, size, type, ptr)		\
static inline void name(type *ptr)			\
{							\
	asm volatile(BUS_LOCK "dec" size " %0"		\
			: "=m" (*ptr)			\
			:  "m" (*ptr));			\
}
build_atomic_dec(atomic_dec, "l", int, p)
build_atomic_dec(atomic_dec64, "q", long, p)

/*
 *  #define atomic_set_int(P, V)		(*(unsigned int *)(P) |= (V))
 */
static inline void atomic_set_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "orl %1,%0"
			:  "+m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_clear_int(P, V)		(*(unsigned int *)(P) &= ~(V))
 */
static inline void atomic_clear_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "andl %1,%0"
			:  "+m" (*p)
			:  "r" (~v)
			:  "cc", "memory");
}

/*
 *  #define atomic_set_long(P, V)		(*(unsigned long *)(P) |= (V))
 */
static inline void atomic_set_long(unsigned long *p, unsigned long v)
{
	__asm __volatile(BUS_LOCK "orq %1,%0"
			:  "+m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_clear_long(P, V)	(*(u_long *)(P) &= ~(V))
 */
static inline void atomic_clear_long(unsigned long *p, unsigned long v)
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
build_atomic_swap(atomic_swap, "l", int, p, v)
build_atomic_swap(atomic_swap64, "q", long, p, v)

 /*
 * #define atomic_readandclear(P) \
 * (return (*(int *)(P)); *(int *)(P) = 0;)
  */
#define atomic_readandclear(p)		atomic_swap(p, 0)

 /*
 * #define atomic_readandclear64(P) \
 * (return (*(long *)(P)); *(long *)(P) = 0;)
  */
#define atomic_readandclear64(p)	atomic_swap64(p, 0)

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
build_atomic_cmpxchg(atomic_cmpxchg, "l", int, p, old, new)
build_atomic_cmpxchg(atomic_cmpxchg64, "q", long, p, old, new)

#define build_atomic_xadd(name, size, type, ptr, v)		\
static inline type name(type *ptr, type v)			\
{								\
	asm volatile(BUS_LOCK "xadd" size " %0,%1"		\
			: "+r" (v), "+m" (*p)			\
			:					\
			: "cc", "memory");			\
	return v;						\
 }
build_atomic_xadd(atomic_xadd, "l", int, p, v)
build_atomic_xadd(atomic_xadd64, "q", long, p, v)

#define atomic_add_return(p, v)		( atomic_xadd(p, v) + v )
#define atomic_sub_return(p, v)		( atomic_xadd(p, -v) - v )

#define atomic_inc_return(v)		atomic_add_return((v), 1)
#define atomic_dec_return(v)		atomic_sub_return((v), 1)

#define atomic_add64_return(p, v)	( atomic_xadd64(p, v) + v )
#define atomic_sub64_return(p, v)	( atomic_xadd64(p, -v) - v )

#define atomic_inc64_return(v)		atomic_add64_return((v), 1)
#define atomic_dec64_return(v)		atomic_sub64_return((v), 1)

#endif /* ATOMIC_H*/
