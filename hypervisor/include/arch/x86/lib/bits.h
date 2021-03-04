/*-
 * Copyright (c) 1998 Doug Rabson
 * Copyright (c) 2017 Intel Corporation
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

#ifndef BITS_H
#define BITS_H
#include <x86/lib/atomic.h>

/**
 *
 * INVALID_BIT_INDEX means when input paramter is zero,
 * bit operations function can't find bit set and return
 * the invalid bit index directly.
 *
 **/
#define INVALID_BIT_INDEX  0xffffU

/*
 *
 * fls32 - Find the Last (most significant) bit Set in value and
 *       return the bit index of that bit.
 *
 *  Bits are numbered starting at 0,the least significant bit.
 *  A return value of INVALID_BIT_INDEX means return value is
 *  invalid bit index when the input argument was zero.
 *
 *  Examples:
 *	fls32 (0x0) = INVALID_BIT_INDEX
 *	fls32 (0x01) = 0
 *	fls32 (0x80) = 7
 *	...
 *	fls32 (0x80000001) = 31
 *
 * @param value: 'uint32_t' type value
 *
 * @return value: zero-based bit index, INVALID_BIT_INDEX means
 * when 'value' was zero, bit operations function can't find bit
 * set and return the invalid bit index directly.
 *
 */
static inline uint16_t fls32(uint32_t value)
{
	uint32_t ret;
	asm volatile("bsrl %1,%0\n\t"
			"jnz 1f\n\t"
			"mov %2,%0\n"
			"1:" : "=r" (ret)
			: "rm" (value), "i" (INVALID_BIT_INDEX));
	return (uint16_t)ret;
}

static inline uint16_t fls64(uint64_t value)
{
	uint64_t ret = 0UL;
	asm volatile("bsrq %1,%0\n\t"
			"jnz 1f\n\t"
			"mov %2,%0\n"
			"1:" : "=r" (ret)
			: "rm" (value), "i" (INVALID_BIT_INDEX));
	return (uint16_t)ret;
}

/*
 *
 * ffs64 - Find the First (least significant) bit Set in value(Long type)
 * and return the index of that bit.
 *
 *  Bits are numbered starting at 0,the least significant bit.
 *  A return value of INVALID_BIT_INDEX means that the return value is the inalid
 *  bit index when the input argument was zero.
 *
 *  Examples:
 *	ffs64 (0x0) = INVALID_BIT_INDEX
 *	ffs64 (0x01) = 0
 *	ffs64 (0xf0) = 4
 *	ffs64 (0xf00) = 8
 *	...
 *	ffs64 (0x8000000000000001) = 0
 *	ffs64 (0xf000000000000000) = 60
 *
 * @param value: 'uint64_t' type value
 *
 * @return value: zero-based bit index, INVALID_BIT_INDEX means
 * when 'value' was zero, bit operations function can't find bit
 * set and return the invalid bit index directly.
 *
 */
static inline uint16_t ffs64(uint64_t value)
{
	uint64_t ret;
	asm volatile("bsfq %1,%0\n\t"
			"jnz 1f\n\t"
			"mov %2,%0\n"
			"1:" : "=r" (ret)
			: "rm" (value), "i" (INVALID_BIT_INDEX));
	return (uint16_t)ret;
}

/*bit scan forward for the least significant bit '0'*/
static inline uint16_t ffz64(uint64_t value)
{
	return ffs64(~value);
}


/*
 * find the first zero bit in a uint64_t array.
 * @pre: the size must be multiple of 64.
 */
static inline uint64_t ffz64_ex(const uint64_t *addr, uint64_t size)
{
	uint64_t ret = size;
	uint64_t idx;

	for (idx = 0UL; (idx << 6U) < size; idx++) {
		if (addr[idx] != ~0UL) {
			ret = (idx << 6U) + ffz64(addr[idx]);
			break;
		}
	}

	return ret;
}
/*
 * Counts leading zeros.
 *
 * The number of leading zeros is defined as the number of
 * most significant bits which are not '1'. E.g.:
 *    clz(0x80000000)==0
 *    clz(0x40000000)==1
 *       ...
 *    clz(0x00000001)==31
 *    clz(0x00000000)==32
 *
 * @param value:The 32 bit value to count the number of leading zeros.
 *
 * @return The number of leading zeros in 'value'.
 */
static inline uint16_t clz(uint32_t value)
{
	return ((value != 0U) ? (31U - fls32(value)) : 32U);
}

/*
 * Counts leading zeros (64 bit version).
 *
 * @param value:The 64 bit value to count the number of leading zeros.
 *
 * @return The number of leading zeros in 'value'.
 */
static inline uint16_t clz64(uint64_t value)
{
	return ((value != 0UL) ? (63U - fls64(value)) : 64U);
}

/*
 * (*addr) |= (1UL<<nr);
 * Note:Input parameter nr shall be less than 64. 
 * If nr>=64, it will be truncated.
 */
#define build_bitmap_set(name, op_len, op_type, lock)			\
static inline void name(uint16_t nr_arg, volatile op_type *addr)	\
{									\
	uint16_t nr;							\
	nr = nr_arg & ((8U * sizeof(op_type)) - 1U);			\
	asm volatile(lock "or" op_len " %1,%0"				\
			:  "+m" (*addr)					\
			:  "r" ((op_type)(1UL<<nr))			\
			:  "cc", "memory");				\
}
build_bitmap_set(bitmap_set_nolock, "q", uint64_t, "")
build_bitmap_set(bitmap_set_lock, "q", uint64_t, BUS_LOCK)
build_bitmap_set(bitmap32_set_nolock, "l", uint32_t, "")
build_bitmap_set(bitmap32_set_lock, "l", uint32_t, BUS_LOCK)

/*
 * (*addr) &= ~(1UL<<nr);
 * Note:Input parameter nr shall be less than 64. 
 * If nr>=64, it will be truncated.
 */
#define build_bitmap_clear(name, op_len, op_type, lock)			\
static inline void name(uint16_t nr_arg, volatile op_type *addr)	\
{									\
	uint16_t nr;							\
	nr = nr_arg & ((8U * sizeof(op_type)) - 1U);			\
	asm volatile(lock "and" op_len " %1,%0"				\
			:  "+m" (*addr)					\
			:  "r" ((op_type)(~(1UL<<(nr))))		\
			:  "cc", "memory");				\
}
build_bitmap_clear(bitmap_clear_nolock, "q", uint64_t, "")
build_bitmap_clear(bitmap_clear_lock, "q", uint64_t, BUS_LOCK)
build_bitmap_clear(bitmap32_clear_nolock, "l", uint32_t, "")
build_bitmap_clear(bitmap32_clear_lock, "l", uint32_t, BUS_LOCK)

/*
 * return !!((*addr) & (1UL<<nr));
 * Note:Input parameter nr shall be less than 64. If nr>=64, it will
 * be truncated.
 */
static inline bool bitmap_test(uint16_t nr, const volatile uint64_t *addr)
{
	int32_t ret = 0;
	asm volatile("btq %q2,%1\n\tsbbl %0, %0"
			: "=r" (ret)
			: "m" (*addr), "r" ((uint64_t)(nr & 0x3fU))
			: "cc", "memory");
	return (ret != 0);
}

static inline bool bitmap32_test(uint16_t nr, const volatile uint32_t *addr)
{
	int32_t ret = 0;
	asm volatile("btl %2,%1\n\tsbbl %0, %0"
			: "=r" (ret)
			: "m" (*addr), "r" ((uint32_t)(nr & 0x1fU))
			: "cc", "memory");
	return (ret != 0);
}

/*
 * bool ret = (*addr) & (1UL<<nr);
 * (*addr) |= (1UL<<nr);
 * return ret;
 * Note:Input parameter nr shall be less than 64. If nr>=64, it
 * will be truncated.
 */
#define build_bitmap_testandset(name, op_len, op_type, lock)		\
static inline bool name(uint16_t nr_arg, volatile op_type *addr)	\
{									\
	uint16_t nr;							\
	int32_t ret=0;							\
	nr = nr_arg & ((8U * sizeof(op_type)) - 1U);			\
	asm volatile(lock "bts" op_len " %2,%1\n\tsbbl %0,%0"		\
			: "=r" (ret), "=m" (*addr)			\
			: "r" ((op_type)nr)				\
			: "cc", "memory");				\
	return (ret != 0);						\
}
build_bitmap_testandset(bitmap_test_and_set_nolock, "q", uint64_t, "")
build_bitmap_testandset(bitmap_test_and_set_lock, "q", uint64_t, BUS_LOCK)
build_bitmap_testandset(bitmap32_test_and_set_nolock, "l", uint32_t, "")
build_bitmap_testandset(bitmap32_test_and_set_lock, "l", uint32_t, BUS_LOCK)

/*
 * bool ret = (*addr) & (1UL<<nr);
 * (*addr) &= ~(1UL<<nr);
 * return ret;
 * Note:Input parameter nr shall be less than 64. If nr>=64,
 * it will be truncated.
 */
#define build_bitmap_testandclear(name, op_len, op_type, lock)		\
static inline bool name(uint16_t nr_arg, volatile op_type *addr)	\
{									\
	uint16_t nr;							\
	int32_t ret=0;							\
	nr = nr_arg & ((8U * sizeof(op_type)) - 1U);			\
	asm volatile(lock "btr" op_len " %2,%1\n\tsbbl %0,%0"		\
			: "=r" (ret), "=m" (*addr)			\
			: "r" ((op_type)nr)				\
			: "cc", "memory");				\
	return (ret != 0);						\
}
build_bitmap_testandclear(bitmap_test_and_clear_nolock, "q", uint64_t, "")
build_bitmap_testandclear(bitmap_test_and_clear_lock, "q", uint64_t, BUS_LOCK)
build_bitmap_testandclear(bitmap32_test_and_clear_nolock, "l", uint32_t, "")
build_bitmap_testandclear(bitmap32_test_and_clear_lock, "l", uint32_t, BUS_LOCK)

static inline uint16_t bitmap_weight(uint64_t bits)
{
	return __builtin_popcountl(bits);
}

#endif /* BITS_H*/
