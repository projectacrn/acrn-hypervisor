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

#define	BUS_LOCK	"lock ; "
/**
 *
 * INVALID_BIT_INDEX means when input paramter is zero,
 * bit operations function can't find bit set and return
 * the invalid bit index directly.
 *
 **/
#define INVALID_BIT_INDEX  0xffffU

/**
 *
 * fls - Find the Last (most significant) bit Set in value and
 *       return the bit index of that bit.
 *
 *  Bits are numbered starting at 0,the least significant bit.
 *  A return value of INVALID_BIT_INDEX means return value is
 *  invalid bit index when the input argument was zero.
 *
 *  Examples:
 *	fls (0x0) = INVALID_BIT_INDEX
 *	fls (0x01) = 0
 *	fls (0x80) = 7
 *	...
 *	fls (0x80000001) = 31
 *
 * @param value: 'uint32_t' type value
 *
 * @return value: zero-based bit index, INVALID_BIT_INDEX means
 * when 'value' was zero, bit operations function can't find bit
 * set and return the invalid bit index directly.
 *
 * **/
static inline uint16_t fls(uint32_t value)
{
	uint32_t ret = 0U;
	if (value == 0U)
		return (INVALID_BIT_INDEX);
	asm volatile("bsrl %1,%0"
			: "=r" (ret)
			: "rm" (value));
	return (uint16_t)ret;
}

static inline uint16_t fls64(uint64_t value)
{
	uint64_t ret = 0UL;
	if (value == 0UL)
		return (INVALID_BIT_INDEX);
	asm volatile("bsrq %1,%0"
			: "=r" (ret)
			: "rm" (value));
	return (uint16_t)ret;
}

/**
 *
 * ffs64 - Find the First (least significant) bit Set in value(Long type)
 * and return the index of that bit.
 *
 *  Bits are numbered starting at 0,the least significant bit.
 *  A return value of -1 means that the argument was zero.
 *
 *  Examples:
 *	ffs64 (0x0) = -1
 *	ffs64 (0x01) = 0
 *	ffs64 (0xf0) = 4
 *	ffs64 (0xf00) = 8
 *	...
 *	ffs64 (0x8000000000000001) = 0
 *	ffs64 (0xf000000000000000) = 60
 *
 * @param value: 'unsigned long' type value
 *
 * @return value: zero-based bit index, -1 means 'value' was zero.
 *
 * **/
static inline int ffs64(unsigned long value)
{
	int ret;
	asm volatile("bsfq %1,%q0"
			: "=r" (ret)
			: "rm" (value), "0" (-1));
	return ret;
}

/*bit scan forward for the least significant bit '0'*/
static inline int ffz64(unsigned long value)
{
	return ffs64(~value);
}

/**
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
	if (value == 0U)
		return 32U;
	else{
		return (31U - fls(value));
	}
}

/**
 * Counts leading zeros (64 bit version).
 *
 * @param value:The 64 bit value to count the number of leading zeros.
 *
 * @return The number of leading zeros in 'value'.
 */
static inline uint16_t clz64(uint64_t value)
{
	if (value == 0UL)
		return 64U;
	else{
		return (63U - fls64(value));
	}
}

/*
 * (*addr) |= (1UL<<nr);
 * Note:Input parameter nr shall be less than 64. 
 * If nr>=64, it will be truncated.
 */
#define build_bitmap_set(name, lock, nr, addr)		\
static inline void name(uint16_t nr, volatile uint64_t *addr)	\
{							\
	nr = nr & 0x3fU;				\
	asm volatile(lock "orq %q1,%0"			\
			:  "+m" (*addr)			\
			:  "r" ((1UL<<(uint64_t)nr))	\
			:  "cc", "memory");		\
}
build_bitmap_set(__bitmap_set, "", nr, addr)
build_bitmap_set(bitmap_set, BUS_LOCK, nr, addr)

/*
 * (*addr) &= ~(1UL<<nr);
 * Note:Input parameter nr shall be less than 64. 
 * If nr>=64, it will be truncated.
 */
#define build_bitmap_clear(name, lock, nr, addr)	\
static inline void name(uint16_t nr, volatile uint64_t *addr)	\
{							\
	nr = nr & 0x3fU;				\
	asm volatile(lock "andq %q1,%0"			\
			:  "+m" (*addr)			\
			:  "r" ((~(1UL<<(uint64_t)nr)))	\
			:  "cc", "memory");		\
}
build_bitmap_clear(__bitmap_clear, "", nr, addr)
build_bitmap_clear(bitmap_clear, BUS_LOCK, nr, addr)

/*
 * return !!((*addr) & (1UL<<nr));
 * Note:Input parameter nr shall be less than 64. If nr>=64, it will
 * be truncated.
 */
static inline bool bitmap_test(uint16_t nr, volatile uint64_t *addr)
{
	int32_t ret=0;
	nr = nr & 0x3fU;
	asm volatile("btq %q2,%1\n\tsbbl %0, %0"
			: "=r" (ret), "=m" (*addr)
			: "r" ((uint64_t)nr)
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
#define build_bitmap_testandset(name, lock, nr, addr)	\
static inline bool name(uint16_t nr, volatile uint64_t *addr)	\
{							\
	int32_t ret=0;					\
	nr = nr & 0x3fU;					\
	asm volatile(lock "btsq %q2,%1\n\tsbbl %0,%0"	\
			: "=r" (ret), "=m" (*addr)	\
			: "r" ((uint64_t)nr)	\
			: "cc", "memory");		\
	return (ret != 0);			        \
}
build_bitmap_testandset(__bitmap_test_and_set, "", nr, addr)
build_bitmap_testandset(bitmap_test_and_set, BUS_LOCK, nr, addr)

/*
 * bool ret = (*addr) & (1UL<<nr);
 * (*addr) &= ~(1UL<<nr);
 * return ret;
 * Note:Input parameter nr shall be less than 64. If nr>=64,
 * it will be truncated.
 */
#define build_bitmap_testandclear(name, lock, nr, addr)	\
static inline bool name(uint16_t nr, volatile uint64_t *addr)	\
{							\
	int32_t ret=0;					\
	nr = nr & 0x3fU;					\
	asm volatile(lock "btrq %q2,%1\n\tsbbl %0,%0"	\
			: "=r" (ret), "=m" (*addr)	\
			: "r" ((uint64_t)nr)	\
			: "cc", "memory");		\
	return (ret != 0);			        \
}
build_bitmap_testandclear(__bitmap_test_and_clear, "", nr, addr)
build_bitmap_testandclear(bitmap_test_and_clear, BUS_LOCK, nr, addr)

#endif /* BITS_H*/
