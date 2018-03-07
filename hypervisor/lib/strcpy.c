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

/**
 *strcpy_s
 *
 * description:
 *    This function copies the string pointed to by s to a buffer
 *    pointed by d.
 *
 *
 * input:
 *    d	       pointer to dest buffer.
 *
 *    dmax     maximum length of dest buffer
 *
 *    s        pointer to the source string
 *
 * return value:
 *    dest      pointer to dest string if string is copied
 *              successfully,or else return null.
 *
 * notes:
 *    1) both d and s shall not be null pointers.
 *    2) dmax shall not 0.
 */
char *strcpy_s(char *d, size_t dmax, const char *s)
{

	char *dest_base;
	size_t dest_avail;
	uint64_t overlap_guard;

	ASSERT(s != NULL, "invalid input s.");
	ASSERT((d != NULL) && (dmax != 0), "invalid input d or dmax.");

	if (s == d)
		return d;

	overlap_guard = (uint64_t)((d > s) ? (d - s - 1) : (s - d - 1));

	dest_avail = dmax;
	dest_base = d;

	while (dest_avail > 0) {
		ASSERT(overlap_guard != 0, "overlap happened.");

		*d = *s;
		if (*d == '\0')
			return dest_base;

		d++;
		s++;
		dest_avail--;
		overlap_guard--;
	}

	ASSERT(false, "dest buffer has no enough space.");

	/*
	 * to avoid a string that is not
	 * null-terminated in dest buffer
	 */
	dest_base[dmax - 1] = '\0';
	return NULL;
}
