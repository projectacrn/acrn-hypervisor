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
/*
 *   #define atomic_set_char(P, V)		(*(unsigned char *)(P) |= (V))
 */
static inline void atomic_set_char(unsigned char *p, unsigned char v)
{
	__asm __volatile(BUS_LOCK    "orb %b1,%0"
			:  "+m" (*p)
			:  "iq" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_clear_char(P, V)		(*(unsigned char *)(P) &= ~(V))
 */
static inline void atomic_clear_char(unsigned char *p, unsigned char v)
{
	__asm __volatile(BUS_LOCK    "andb %b1,%0"
			:  "+m" (*p)
			:  "iq" (~v)
			:  "cc", "memory");
}

/*
 *  #define atomic_add_char(P, V)		(*(unsigned char *)(P) += (V))
 */
static inline void atomic_add_char(unsigned char *p, unsigned char v)
{
	__asm __volatile(BUS_LOCK    "addb %b1,%0"
			:  "+m" (*p)
			:  "iq" (v)
			:  "cc", "memory");
}

/*
 * #define atomic_subtract_char(P, V)	(*(unsigned char *)(P) -= (V))
 */
static inline void atomic_subtract_char(unsigned char *p, unsigned char v)
{
	__asm __volatile(BUS_LOCK "subb %b1,%0"
			:  "+m" (*p)
			:  "iq" (v)
			:  "cc", "memory");
}

/*
 * #define atomic_set_short(P, V)		(*(unsigned short *)(P) |= (V))
 */
static inline void atomic_set_short(unsigned short *p, unsigned short v)
{
	__asm __volatile(BUS_LOCK "orw %w1,%0"
			:  "+m" (*p)
			:  "ir" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_clear_short(P, V)	(*(unsigned short *)(P) &= ~(V))
 */
static inline void atomic_clear_short(unsigned short *p, unsigned short v)
{
	__asm __volatile(BUS_LOCK "andw %w1,%0"
			:  "+m" (*p)
			:  "ir" (~v)
			:  "cc", "memory");
}

/*
 *  #define atomic_add_short(P, V)		(*(unsigned short *)(P) += (V))
 */
static inline void atomic_add_short(unsigned short *p, unsigned short v)
{
	__asm __volatile(BUS_LOCK "addw %w1,%0"
			:  "+m" (*p)
			:  "ir" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_subtract_short(P, V)	(*(unsigned short *)(P) -= (V))
 */
static inline void atomic_subtract_short(unsigned short *p, unsigned short v)
{
	__asm __volatile(BUS_LOCK "subw %w1,%0"
			:  "+m" (*p)
			:  "ir" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_set_int(P, V)		(*(unsigned int *)(P) |= (V))
 */
static inline void atomic_set_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "orl %1,%0"
			:  "+m" (*p)
			:  "ir" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_clear_int(P, V)		(*(unsigned int *)(P) &= ~(V))
 */
static inline void atomic_clear_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "andl %1,%0"
			:  "+m" (*p)
			:  "ir" (~v)
			:  "cc", "memory");
}

/*
 *  #define atomic_add_int(P, V)		(*(unsigned int *)(P) += (V))
 */
static inline void atomic_add_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "addl %1,%0"
			:  "+m" (*p)
			:  "ir" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_subtract_int(P, V)	(*(unsigned int *)(P) -= (V))
 */
static inline void atomic_subtract_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "subl %1,%0"
			:  "+m" (*p)
			:  "ir" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_swap_int(P, V) \
 *  (return (*(unsigned int *)(P)); *(unsigned int *)(P) = (V);)
 */
static inline int atomic_swap_int(unsigned int *p, unsigned int v)
{
	__asm __volatile(BUS_LOCK "xchgl %1,%0"
			:  "+m" (*p), "+r" (v)
			:
			:  "cc", "memory");
	return v;
}

/*
 *  #define atomic_readandclear_int(P) \
 *  (return (*(unsigned int *)(P)); *(unsigned int *)(P) = 0;)
 */
#define	atomic_readandclear_int(p)	atomic_swap_int(p, 0)

/*
 *  #define atomic_set_long(P, V)		(*(unsigned long *)(P) |= (V))
 */
static inline void atomic_set_long(unsigned long *p, unsigned long v)
{
	__asm __volatile(BUS_LOCK "orq %1,%0"
			:  "+m" (*p)
			:  "ir" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_clear_long(P, V)	(*(u_long *)(P) &= ~(V))
 */
static inline void atomic_clear_long(unsigned long *p, unsigned long v)
{
	__asm __volatile(BUS_LOCK "andq %1,%0"
			:  "+m" (*p)
			:  "ir" (~v)
			:  "cc", "memory");
}

/*
 *  #define atomic_add_long(P, V)		(*(unsigned long *)(P) += (V))
 */
static inline void atomic_add_long(unsigned long *p, unsigned long v)
{
	__asm __volatile(BUS_LOCK "addq %1,%0"
			:  "+m" (*p)
			:  "ir" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_subtract_long(P, V)	(*(unsigned long *)(P) -= (V))
 */
static inline void atomic_subtract_long(unsigned long *p, unsigned long v)
{
	__asm __volatile(BUS_LOCK "subq %1,%0"
			:  "+m" (*p)
			:  "ir" (v)
			:  "cc", "memory");
}

/*
 *  #define atomic_swap_long(P, V) \
 *  (return (*(unsigned long *)(P)); *(unsigned long *)(P) = (V);)
 */
static inline long atomic_swap_long(unsigned long *p, unsigned long v)
{
	__asm __volatile(BUS_LOCK "xchgq %1,%0"
			:  "+m" (*p), "+r" (v)
			:
			:  "cc", "memory");
	return v;
}

/*
 *  #define atomic_readandclear_long(P) \
 *  (return (*(unsigned long *)(P)); *(unsigned long *)(P) = 0;)
 */
#define	atomic_readandclear_long(p)	atomic_swap_long(p, 0)

/*
 *   #define atomic_load_acq_int(P)		(*(unsigned int*)(P))
 */
static inline int atomic_load_acq_int(unsigned int *p)
{
	int ret;

	__asm __volatile("movl %1,%0"
			:  "=r"(ret)
			:  "m" (*p)
			:  "cc", "memory");
	return ret;
}

/*
 *   #define atomic_store_rel_int(P, V)	(*(unsigned int *)(P) = (V))
 */
static inline void atomic_store_rel_int(unsigned int *p, unsigned int v)
{
	__asm __volatile("movl %1,%0"
			:  "=m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

/*
 *   #define atomic_load_acq_long(P)		(*(unsigned long*)(P))
 */
static inline long atomic_load_acq_long(unsigned long *p)
{
	long ret;

	__asm __volatile("movq %1,%0"
			:  "=r"(ret)
			:  "m" (*p)
			:  "cc", "memory");
	return ret;
}

/*
 *   #define atomic_store_rel_long(P, V)	(*(unsigned long *)(P) = (V))
 */
static inline void atomic_store_rel_long(unsigned long *p, unsigned long v)
{
	__asm __volatile("movq %1,%0"
			:  "=m" (*p)
			:  "r" (v)
			:  "cc", "memory");
}

static inline int atomic_cmpxchg_int(unsigned int *p,
			int old, int new)
{
	int ret;

	__asm __volatile(BUS_LOCK "cmpxchgl %2,%1"
			: "=a" (ret), "+m" (*p)
			: "r" (new), "0" (old)
			: "memory");

	return ret;
}

#define atomic_load_acq_32		atomic_load_acq_int
#define atomic_store_rel_32		atomic_store_rel_int
#define atomic_load_acq_64		atomic_load_acq_long
#define atomic_store_rel_64		atomic_store_rel_long

/*
 *  #define atomic_xadd_int(P, V) \
 *  (return (*(unsigned long *)(P)); *(unsigned long *)(P) += (V);)
 */
static inline int atomic_xadd_int(unsigned int *p, unsigned int  v)
{
	__asm __volatile(BUS_LOCK  "xaddl %0,%1"
			:  "+r" (v), "+m" (*p)
			:
			:  "cc", "memory");
	return v;
}

static inline int atomic_add_return(int v, unsigned int *p)
{
	return v + atomic_xadd_int(p, v);
}

static inline int atomic_sub_return(int v, unsigned int *p)
{
	return atomic_xadd_int(p, -v) - v;
}

#define	atomic_inc_return(v)		atomic_add_return(1, (v))
#define	atomic_dec_return(v)		atomic_sub_return(1, (v))

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
			:  "ir" (1UL<<mask)
			:  "cc", "memory");
}

static inline void
bitmap_clr(int mask, unsigned long *bits)
{
	/* (*bits) &= ~(1UL<<mask); */
	__asm __volatile(BUS_LOCK "andq %1,%0"
			:  "+m" (*bits)
			:  "ir" (~(1UL<<mask))
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
			: "ir" ((long)(mask) & 0x3f)
			: "cc", "memory");
	return (!!ret);
}

static inline int
bitmap_test_and_set(int mask, unsigned long *bits)
{
	int ret;

	__asm __volatile(BUS_LOCK  "btsq %2,%1\n\tsbbl %0,%0"
			: "=r" (ret), "=m" (*bits)
			: "ir" ((long)(mask & 0x3f))
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
			: "ir" ((long)(mask) & 0x3f)
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
			:  "ir" ((1UL<<mask))
			:  "cc", "memory");

}

static inline int
bitmap_ffs(unsigned long *bits)
{
	return ffsl(*bits);
}

static inline int
atomic_cmpset_long(unsigned long *dst, unsigned long expect, unsigned long src)
{
	unsigned char res;

	__asm __volatile(BUS_LOCK "cmpxchg %3,%1\n\tsete %0"
			: "=q" (res), "+m" (*dst), "+a" (expect)
			: "r" (src)
			: "memory", "cc");

	return res;
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
