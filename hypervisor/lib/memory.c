/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */
#include <types.h>
#include <memory.h>

#ifndef HAS_ARCH_MEMORY_LIB
void *memset(void *base, uint8_t v, size_t n)
{
    uint8_t *p = (uint8_t *)base;

    for (size_t i = 0U; i < n; i++) {
        *p++ = v;
    }

    return base;
}

void *memset_s(void *base, uint8_t v, size_t n)
{
    if ((base != NULL) && (n != 0U)) {
        (void)memset(base, v, n);
    }

    return base;
}

void memcpy(void *d, const void *s, size_t slen)
{
    uint8_t *dst = (uint8_t *)d;
    const uint8_t *src = (const uint8_t *)s;

    for (size_t i = 0U; i < slen; i++) {
        *dst++ = *src++;
    }
}

void memcpy_backwards(void *d, const void *s, size_t slen)
{
	uint8_t *dst = (uint8_t *)d + slen - 1;
	const uint8_t *src = (const uint8_t *)s + slen - 1;

	for (size_t i = 0U; i < slen; i++) {
		*dst-- = *src--;
	}
}
#endif

int32_t memcpy_s(void *d, size_t dmax, const void *s, size_t slen)
{
	int32_t ret = -1;

	if ((d != NULL) && (s != NULL) && (dmax >= slen) && ((d > (s + slen)) || (s > (d + dmax)))) {
		if (slen != 0U) {
			memcpy(d, s, slen);
		}
		ret = 0;
	} else {
		(void)memset(d, 0U, dmax);
	}

	return ret;
}
