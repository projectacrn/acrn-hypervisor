/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 *   Haoyu Tang <haoyu.tang@intel.com>
 */

#ifndef RISCV_LIB_ATOMIC_H
#define RISCV_LIB_ATOMIC_H

#define build_atomic_inc(name, type, size)			\
static inline void arch_atomic_##name(type *ptr)		\
{								\
	type inc = 1;						\
	asm volatile("amoadd." size " zero, %1, %0"		\
			: "+A" (*ptr)				\
			: "r" (inc)				\
			: "memory");				\
}
build_atomic_inc(inc32, uint32_t, "w")
build_atomic_inc(inc64, uint64_t, "d")

#define build_atomic_dec(name, type, size)			\
static inline void arch_atomic_##name(type *ptr)		\
{								\
	type dec = -1;						\
	asm volatile("amoadd." size " zero, %1, %0"		\
			: "+A" (*ptr)				\
			: "r" (dec)				\
			: "memory");				\
}
build_atomic_dec(dec32, uint32_t, "w")
build_atomic_dec(dec64, uint64_t, "d")

#define build_atomic_swap(name, type, size)			\
static inline type arch_atomic_##name(type *ptr, type v)	\
{								\
	asm volatile("amoswap." size " %0, %2, %1"		\
			: "=r" (v), "+A" (*ptr)			\
			: "r" (v)				\
			: "memory");				\
	return v;						\
}
build_atomic_swap(swap32, uint32_t, "w")
build_atomic_swap(swap64, uint64_t, "d")

#define build_atomic_op(name, size, type, op)				\
static inline type arch_atomic_##name##_return(type *v, type i)		\
{									\
	type ret;							\
	asm volatile("amoadd." size ".aqrl %1, %2, %0\n\t"		\
			: "+A"(*v), "=r"(ret)				\
			: "r"(op i)					\
			: "memory");					\
	return ret op i;						\
}
build_atomic_op(add, "w", int32_t, +)
build_atomic_op(add64, "d", int64_t, +)
build_atomic_op(sub, "w", int32_t, -)
build_atomic_op(sub64, "d", int64_t, -)

static inline int32_t arch_atomic_inc_return(int32_t *v)
{
	return  arch_atomic_add_return(v, 1);
}

static inline int32_t arch_atomic_dec_return(int32_t *v)
{
	return arch_atomic_sub_return(v, 1);
}

static inline int64_t arch_atomic_inc64_return(int64_t *v)
{
	return arch_atomic_add64_return(v, 1);
}

static inline int64_t arch_atomic_dec64_return(int64_t *v)
{
	return arch_atomic_sub64_return(v, 1);
}

#ifdef RISCV_ISA_EXT_ZACAS /* extension `zacas' is required */
#define build_cmpxchg(name, size, type)							\
static inline type arch_atomic_##name(volatile type *ptr, type old, type new)		\
{											\
	type ret;									\
	ret = old;									\
	asm volatile("amocas." size ".aqrl %0, %z2, %1\n"				\
			: "+&r"(ret), "+A"(*ptr)					\
			: "rJ"(new)							\
			: "memory");							\
	return ret;									\
}
#else /* RISCV_ISA_EXT_ZACAS not defined */
#define build_cmpxchg(name, size, type)							\
static inline type arch_atomic_##name(volatile type *ptr, type old, type new)		\
{											\
	type ret;									\
	register type rc;								\
	asm volatile("0: lr." size " %0, %2\n"						\
			" bne %0, %z3, 1f\n"						\
			" sc." size ".rl %1, %z4, %2\n"					\
			" bnez %1, 0b\n"						\
			" fence rw, rw\n"						\
			"1:\n"								\
			: "=&r"(ret), "=&r"(rc), "+A"(*ptr)				\
			: "rJ"(old), "rJ"(new)						\
			: "memory");							\
	return ret;									\
}
#endif /* RISCV_ISA_EXT_ZACAS */
build_cmpxchg(cmpxchg32, "w", uint32_t)
build_cmpxchg(cmpxchg64, "d", uint64_t)

#endif /* RISCV_LIB_ATOMIC_H */