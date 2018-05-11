/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>


/***********************************************************************
 *
 *   FUNCTION
 *
 *       memcpy_s
 *
 *   DESCRIPTION
 *
 *       Copies at most slen bytes from src address to dest address,
 *       up to dmax.
 *
 *   INPUTS
 *
 *       d                  pointer to Destination address
 *       dmax               maximum  length of dest
 *       s                  pointer to Source address
 *       slen               maximum number of bytes of src to copy
 *
 *   OUTPUTS
 *
 *       void *             pointer to destination address if successful,
 * 			    or else return null.
 *
 ***********************************************************************/
void *memcpy_s(void *d, size_t dmax, const void *s, size_t slen)
{

	uint8_t *dest8;
	uint8_t *src8;

	if (slen == 0 || dmax == 0 || dmax < slen) {
		pr_err("%s: invalid src, dest buffer or length.", __func__);
		return NULL;
	}

	if ((d > s && d <= s + slen - 1)
		|| (d < s && s <= d + dmax - 1)) {
		pr_err("%s: overlap happened.", __func__);
		return NULL;
	}

	/*same memory block, no need to copy*/
	if (d == s)
		return d;

	dest8 = (uint8_t *)d;
	src8 = (uint8_t *)s;

	/*small data block*/
	if (slen < 8) {
		while (slen) {
			*dest8++ = *src8++;
			slen--;
		}

		return d;
	}

	/*make sure 8bytes-aligned for at least one addr.*/
	if ((!MEM_ALIGNED_CHECK(src8, 8)) && (!MEM_ALIGNED_CHECK(dest8, 8))) {
		for (; slen && (((uint64_t)src8) & 7); slen--)
			*dest8++ = *src8++;
	}

	/*copy main data blocks, with rep prefix*/
	if (slen > 8) {
		uint32_t ecx;

		asm volatile ("cld; rep; movsq"
				: "=&c"(ecx), "=&D"(dest8), "=&S"(src8)
				: "0" (slen / 8), "1" (dest8), "2" (src8)
				: "memory");

		slen = slen % 8;
	}

	/*tail bytes*/
	while (slen) {
		*dest8++ = *src8++;
		slen--;
	}

	return d;
}
