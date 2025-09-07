/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MEMORY_LIB_H
#define MEMORY_LIB_H
#include <types.h>
#include <asm/lib/memory.h>

#ifdef HAS_ARCH_MEMORY_LIB
void *arch_memset(void *base, uint8_t v, size_t n);
void arch_memcpy(void *d, const void *s, size_t slen);
void arch_memcpy_backwards(void *d, const void *s, size_t slen);

static inline void *memset(void *base, uint8_t v, size_t n) {
	return arch_memset(base, v, n);
}

static inline void memcpy(void *d, const void *s, size_t slen) {
	return arch_memcpy(d, s, slen);
}

static inline void memcpy_backwards(void *d, const void *s, size_t slen) {
	return arch_memcpy_backwards(d, s, slen);
}
#else
void *memset(void *base, uint8_t v, size_t n);
void memcpy(void *d, const void *s, size_t slen);
void memcpy_backwards(void *d, const void *s, size_t slen);
#endif

int32_t memcpy_s(void *d, size_t dmax, const void *s, size_t slen);


#endif /* MEMORY_LIB_H */
