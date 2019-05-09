/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>

static inline void memcpy_erms(void *d, const void *s, size_t slen)
{
	asm volatile ("rep; movsb"
		: "=&D"(d), "=&S"(s)
		: "c"(slen), "0" (d), "1" (s)
		: "memory");
}

/*
 * @brief  Copies at most slen bytes from src address to dest address, up to dmax.
 *
 *   INPUTS
 *
 * @param[in] d        pointer to Destination address
 * @param[in] dmax     maximum  length of dest
 * @param[in] s        pointer to Source address
 * @param[in] slen     maximum number of bytes of src to copy
 *
 * @return pointer to destination address.
 *
 * @pre d and s will not overlap.
 */
void *memcpy_s(void *d, size_t dmax, const void *s, size_t slen)
{
	if ((slen != 0U) && (dmax != 0U) && (dmax >= slen)) {
		/* same memory block, no need to copy */
		if (d != s) {
			memcpy_erms(d, s, slen);
		}
	}
	return d;
}

static inline void memset_erms(void *base, uint8_t v, size_t n)
{
	asm volatile("rep ; stosb"
			: "+D"(base)
			: "a" (v), "c"(n));
}

void *memset(void *base, uint8_t v, size_t n)
{
	/*
	 * Some CPUs support enhanced REP MOVSB/STOSB feature. It is recommended
	 * to use it when possible.
	 */
	if ((base != NULL) && (n != 0U)) {
		memset_erms(base, v, n);
        }

	return base;
}
