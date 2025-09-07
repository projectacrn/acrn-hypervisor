/*
 * Copyright (C) 2018-2025 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <memory.h>

static inline void x86_memset(void *base, uint8_t v, size_t n)
{
	asm volatile("rep ; stosb"
		: "+D"(base)
		: "a" (v), "c"(n));
}

static inline void x86_memcpy(void *d, const void *s, size_t slen)
{
	asm volatile ("rep; movsb"
		: "=&D"(d), "=&S"(s)
		: "c"(slen), "0" (d), "1" (s)
		: "memory");
}

static inline void x86_memcpy_backwards(void *d, const void *s, size_t slen)
{
	asm volatile ("std; rep; movsb; cld"
		: "=&D"(d), "=&S"(s)
		: "c"(slen), "0" (d), "1" (s)
		: "memory");
}

void *arch_memset(void *base, uint8_t v, size_t n)
{
	/*
	 * Some CPUs support enhanced REP MOVSB/STOSB feature. It is recommended
	 * to use it when possible.
	 */
	if ((base != NULL) && (n != 0U)) {
		x86_memset(base, v, n);
	}

	return base;
}

void arch_memcpy(void *d, const void *s, size_t slen)
{
	if ((d != NULL) && (s != NULL) && (slen != 0U)) {
		x86_memcpy(d, s, slen);
	}
}

void arch_memcpy_backwards(void *d, const void *s, size_t slen)
{
	if ((d != NULL) && (s != NULL) && (slen != 0U)) {
		x86_memcpy_backwards(d, s, slen);
	}
}
