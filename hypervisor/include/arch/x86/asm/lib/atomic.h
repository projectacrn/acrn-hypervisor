/*-
 * Copyright (c) 1998 Doug Rabson
 * Copyright (c) 2018-2025 Intel Corporation.
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

#ifndef X86_LIB_ATOMIC_H
#define X86_LIB_ATOMIC_H
#define	BUS_LOCK	"lock ; "

#define build_atomic_inc(name, size, type)		\
static inline void name(type *ptr)			\
{							\
	asm volatile(BUS_LOCK "inc" size " %0"		\
			: "=m" (*ptr)			\
			:  "m" (*ptr));			\
}
build_atomic_inc(arch_atomic_inc16, "w", uint16_t)
build_atomic_inc(arch_atomic_inc32, "l", uint32_t)
build_atomic_inc(arch_atomic_inc64, "q", uint64_t)

#define build_atomic_dec(name, size, type)		\
static inline void name(type *ptr)			\
{							\
	asm volatile(BUS_LOCK "dec" size " %0"		\
			: "=m" (*ptr)			\
			:  "m" (*ptr));			\
}
build_atomic_dec(arch_atomic_dec16, "w", uint16_t)
build_atomic_dec(arch_atomic_dec32, "l", uint32_t)
build_atomic_dec(arch_atomic_dec64, "q", uint64_t)

#define build_atomic_swap(name, size, type)		\
static inline type name(type *ptr, type v)		\
{							\
	asm volatile(BUS_LOCK "xchg" size " %1,%0"	\
			:  "+m" (*ptr), "+r" (v)	\
			:				\
			:  "cc", "memory");		\
	return v;					\
}
build_atomic_swap(arch_atomic_swap32, "l", uint32_t)
build_atomic_swap(arch_atomic_swap64, "q", uint64_t)

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
build_atomic_cmpxchg(arch_atomic_cmpxchg32, "l", uint32_t)
build_atomic_cmpxchg(arch_atomic_cmpxchg64, "q", uint64_t)

#define build_atomic_xadd(name, size, type)			\
static inline type name(type *ptr, type v)			\
{								\
	asm volatile(BUS_LOCK "xadd" size " %0,%1"		\
			: "+r" (v), "+m" (*ptr)			\
			:					\
			: "cc", "memory");			\
	return v;						\
 }
build_atomic_xadd(arch_atomic_xadd16, "w", uint16_t)
build_atomic_xadd(arch_atomic_xadd32, "l", int32_t)
build_atomic_xadd(arch_atomic_xadd64, "q", int64_t)

static inline int32_t arch_atomic_add_return(int32_t *p, int32_t v)
{
	return (arch_atomic_xadd32(p, v) + v);
}

static inline int32_t arch_atomic_sub_return(int32_t *p, int32_t v)
{
	return (arch_atomic_xadd32(p, -v) - v);
}

static inline int32_t arch_atomic_inc_return(int32_t *v)
{
	return arch_atomic_add_return(v, 1);
}

static inline int32_t arch_atomic_dec_return(int32_t *v)
{
	return arch_atomic_sub_return(v, 1);
}

static inline int64_t arch_atomic_add64_return(int64_t *p, int64_t v)
{
	return (arch_atomic_xadd64(p, v) + v);
}

static inline int64_t arch_atomic_sub64_return(int64_t *p, int64_t v)
{
	return (arch_atomic_xadd64(p, -v) - v);
}

static inline int64_t arch_atomic_inc64_return(int64_t *v)
{
	return arch_atomic_add64_return(v, 1);
}

static inline int64_t arch_atomic_dec64_return(int64_t *v)
{
	return arch_atomic_sub64_return(v, 1);
}

#endif /* X86_LIB_ATOMIC_H */
