#ifndef _TYPES_H_
#define _TYPES_H_

#include "macros.h"
#include <stdint.h>
#include <stdarg.h>
#include <sched.h>
#include <sys/types.h>

#define MAXCOMLEN   19      /* max command name remembered */
#define MAXINTERP   PATH_MAX    /* max interpreter file name length */
#define MAXLOGNAME  33      /* max login name length (incl. NUL) */
#define SPECNAMELEN 63      /* max length of devicename */

typedef cpu_set_t cpuset_t;
typedef uint64_t vm_paddr_t;
typedef uint64_t vm_ooffset_t;
typedef uint64_t cap_ioctl_t;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define container_of(ptr, type, member) ({                   \
	const typeof(((type *)0)->member) * __mptr = (ptr);  \
	(type *)((char *)__mptr - (offsetof(type, member))); \
})

#define __aligned(x)		__attribute__((aligned(x)))
#define __section(x)		__attribute__((__section__(x)))
#define __MAKE_SET(set, sym)                        \
	static void const * const __set_##set##_sym_##sym       \
	__section("set_" #set) __attribute__((used)) = &sym

#define DATA_SET(set, sym)  __MAKE_SET(set, sym)

#define SET_DECLARE(set, ptype)\
	 extern ptype * __CONCAT(__start_set_, set); \
	 extern ptype *__CONCAT(__stop_set_, set)

#define SET_BEGIN(set)                          \
	(&__CONCAT(__start_set_, set))
#define SET_LIMIT(set)                          \
	(&__CONCAT(__stop_set_, set))

#define SET_FOREACH(pvar, set)                      \
	for (pvar = SET_BEGIN(set); pvar < SET_LIMIT(set); pvar++)

#define nitems(x) (sizeof((x)) / sizeof((x)[0]))
#define roundup2(x, y)  (((x)+((y)-1))&(~((y)-1)))
#define rounddown2(x, y) ((x)&(~((y)-1)))

static inline uint16_t
be16dec(const void *pp)
{
	uint8_t const *p = (uint8_t const *)pp;

	return ((p[0] << 8) | p[1]);
}

static inline uint32_t
be32dec(const void *pp)
{
	uint8_t const *p = (uint8_t const *)pp;

	return (((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

static inline void
be16enc(void *pp, uint16_t u)
{
	uint8_t *p = (uint8_t *)pp;

	p[0] = (u >> 8) & 0xff;
	p[1] = u & 0xff;
}

static inline void
be32enc(void *pp, uint32_t u)
{
	uint8_t *p = (uint8_t *)pp;

	p[0] = (u >> 24) & 0xff;
	p[1] = (u >> 16) & 0xff;
	p[2] = (u >> 8) & 0xff;
	p[3] = u & 0xff;
}
static inline int
flsl(uint64_t mask)
{
	return mask ? 64 - __builtin_clzl(mask) : 0;
}

static inline int
fls(uint32_t mask)
{
	return mask ? 32 - __builtin_clz(mask) : 0;
}

/* Returns the number of 1-bits in bits. */
static inline int
bitmap_weight(uint64_t bits)
{
	return __builtin_popcountl(bits);

}

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

/*
 * ffs64 - Find the first (least significant) bit set of value
 * and return the index of that bit.
 *
 * @return value: zero-based bit index, or if value is zero, returns INVALID_BIT_INDEX
 */
#define INVALID_BIT_INDEX  0xffffU
static inline uint16_t ffs64(uint64_t value)
{
	/*
	 * __builtin_ffsl: returns one plus the index of the least significant 1-bit of value,
	 * or if value is zero, returns zero.
	 */
	return value ? (uint16_t)__builtin_ffsl(value) - 1U: INVALID_BIT_INDEX;
}

/* memory barrier */
#define mb()    ({ asm volatile("mfence" ::: "memory"); (void)0; })

static inline void
do_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	__asm __volatile("cpuid"
		: "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
		: "a" (leaf), "c" (subleaf)
		: "memory");
}

/*
 * @brief  Get the order value of given count.
 *
 * @param  num 32-bits value.
 *
 * @return the order value of data on success and -1 on fail.
 *
 */
static inline int get_num_order(uint32_t num)
{
	return ((num > 0) ? fls(num - 1) : -1);
}

#define UGETW(w)            \
	((w)[0] |             \
	(((uint16_t)((w)[1])) << 8))

#define UGETDW(w)           \
	((w)[0] |             \
	(((uint16_t)((w)[1])) << 8) |     \
	(((uint32_t)((w)[2])) << 16) |    \
	(((uint32_t)((w)[3])) << 24))

#define UGETQW(w)           \
	((w)[0] |             \
	(((uint16_t)((w)[1])) << 8) |     \
	(((uint32_t)((w)[2])) << 16) |    \
	(((uint32_t)((w)[3])) << 24) |    \
	(((uint64_t)((w)[4])) << 32) |    \
	(((uint64_t)((w)[5])) << 40) |    \
	(((uint64_t)((w)[6])) << 48) |    \
	(((uint64_t)((w)[7])) << 56))

#define USETW(w, v) do {         \
	  (w)[0] = (uint8_t)(v);        \
	  (w)[1] = (uint8_t)((v) >> 8);     \
} while (0)

#define USETDW(w, v) do {        \
	  (w)[0] = (uint8_t)(v);        \
	  (w)[1] = (uint8_t)((v) >> 8);     \
	  (w)[2] = (uint8_t)((v) >> 16);    \
	  (w)[3] = (uint8_t)((v) >> 24);    \
} while (0)

#define USETQW(w, v) do {        \
	  (w)[0] = (uint8_t)(v);        \
	  (w)[1] = (uint8_t)((v) >> 8);     \
	  (w)[2] = (uint8_t)((v) >> 16);    \
	  (w)[3] = (uint8_t)((v) >> 24);    \
	  (w)[4] = (uint8_t)((v) >> 32);    \
	  (w)[5] = (uint8_t)((v) >> 40);    \
	  (w)[6] = (uint8_t)((v) >> 48);    \
	  (w)[7] = (uint8_t)((v) >> 56);    \
} while (0)

#define __packed       __attribute__((packed))

#endif
