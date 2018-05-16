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

static inline unsigned int
bsrl(unsigned int mask)
{
	unsigned int result;

	__asm __volatile("bsrl %1,%0"
			: "=r" (result)
			: "rm" (mask));
	return result;
}

static inline unsigned long
bsrq(unsigned long mask)
{
	unsigned long result;

	__asm __volatile("bsrq %1,%0"
			: "=r" (result)
			: "rm" (mask));
	return result;
}

/**
 *
 * fls - Find the Last (most significant) bit Set in value and
 *       return the bit index of that bit.
 *
 *  Bits are numbered starting at 0,the least significant bit.
 *  A return value of -1 means that the argument was zero.
 *
 *  Examples:
 *	fls (0x0) = -1
 *	fls (0x01) = 0
 *	fls (0xf0) = 7
 *	...
 *	fls (0x80000001) = 31
 *
 * @param mask: 'int' type value
 *
 * @return value: zero-based bit index, -1 means 'mask' was zero.
 *
 * **/
static inline int
fls(int mask)
{
	return (mask == 0 ? -1 : (int)bsrl((unsigned int)mask));
}

/* 64bit version of fls(). */
static inline int
flsl(long mask)
{
	return (mask == 0 ? -1 : (int)bsrq((unsigned long)mask));
}

static inline unsigned long
bsfq(unsigned long mask)
{
	unsigned long result;

	__asm __volatile("bsfq %1,%0"
			: "=r" (result)
			: "rm" (mask));
	return result;
}

/**
 *
 * ffsl - Find the First (least significant) bit Set in value(Long type)
 * and return the index of that bit.
 *
 *  Bits are numbered starting at 0,the least significant bit.
 *  A return value of -1 means that the argument was zero.
 *
 *  Examples:
 *	ffsl (0x0) = -1
 *	ffsl (0x01) = 0
 *	ffsl (0xf0) = 4
 *	ffsl (0xf00) = 8
 *	...
 *	ffsl (0x8000000000000001) = 0
 *	ffsl (0xf000000000000000) = 60
 *
 * @param mask: 'long' type value
 *
 * @return value: zero-based bit index, -1 means 'mask' was zero.
 *
 * **/
static inline int
ffsl(long mask)
{
	return (mask == 0 ? -1 : (int)bsfq((unsigned long)mask));
}

static inline void
bitmap_set(int mask, unsigned long *bits)
{
	/*  (*bits) |= (1UL<<mask); */
	__asm __volatile(BUS_LOCK "orq %1,%0"
			:  "+m" (*bits)
			:  "r" (1UL<<mask)
			:  "cc", "memory");
}

static inline void
bitmap_clr(int mask, unsigned long *bits)
{
	/* (*bits) &= ~(1UL<<mask); */
	__asm __volatile(BUS_LOCK "andq %1,%0"
			:  "+m" (*bits)
			:  "r" (~(1UL<<mask))
			:  "cc", "memory");
}

static inline int
bitmap_isset(int mask, unsigned long *bits)
{
	/*
	 * return (*bits) & (1UL<<mask);
	 */
	int ret;

	__asm __volatile("btq %2,%1\n\tsbbl %0, %0"
			: "=r" (ret), "=m" (*bits)
			: "r" ((long)(mask) & 0x3f)
			: "cc", "memory");
	return (!!ret);
}

static inline int
bitmap_test_and_set(int mask, unsigned long *bits)
{
	int ret;

	__asm __volatile(BUS_LOCK  "btsq %2,%1\n\tsbbl %0,%0"
			: "=r" (ret), "=m" (*bits)
			: "r" ((long)(mask & 0x3f))
			: "cc", "memory");
	return (!!ret);
}

static inline int
bitmap_test_and_clear(int mask, unsigned long *bits)
{
	/*
	 *    bool ret = (*bits) & (1UL<<mask);
	 *    (*bits) &= ~(1UL<<mask);
	 *    return ret;
	 */
	int ret;

	__asm __volatile(BUS_LOCK  "btrq %2,%1\n\tsbbl %0,%0"
			: "=r" (ret), "=m" (*bits)
			: "r" ((long)(mask) & 0x3f)
			: "cc", "memory");
	return (!!ret);
}

static inline void
bitmap_setof(int mask, unsigned long *bits)
{
	/*
	 *    *bits = 0;
	 *    (*bits) |= (1UL<<mask);
	 */

	__asm __volatile(BUS_LOCK "xchgq %1,%0"
			:  "+m" (*bits)
			:  "r" ((1UL<<mask))
			:  "cc", "memory");

}

static inline int
bitmap_ffs(unsigned long *bits)
{
	return ffsl(*bits);
}

/*bit scan forward for the least significant bit '0'*/
static inline int
get_first_zero_bit(unsigned long value)
{
	return ffsl(~value);
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
 * @param mask:The 32 bit value to count the number of leading zeros.
 *
 * @return The number of leading zeros in 'mask'.
 */
static inline int
clz(int mask)
{
	return (31 - fls(mask));
}

/**
 * Counts leading zeros (64 bit version).
 *
 * @param mask:The 64 bit value to count the number of leading zeros.
 *
 * @return The number of leading zeros in 'mask'.
 */
static inline int
clz64(long mask)
{
	return (63 - flsl(mask));
}

#endif /* BITS_H*/
