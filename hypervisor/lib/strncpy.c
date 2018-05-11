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

/*
 * strncpy_s
 *
 * description:
 *    This function copies maximum 'slen'characters from string pointed
 *    by s to a buffer pointed by d.
 *
 * input:
 *    d         pointer to dest buffer.
 *
 *    dmax      maximum length of dest buffer.
 *
 *    s         pointer to the source string.
 *
 *    slen      the maximum number of characters to copy from source
 *              string.
 *
 * return value:
 *    dest      pointer to dest string if source string is copied
 *              successfully, or else return null.
 *
 * notes:
 *    1) both dmax and slen should not be 0.
 *    2) both d and s should not be null pointers.
 *    3) will assert() if overlap happens or dest buffer has no
 *       enough space.
 */
char *strncpy_s(char *d, size_t dmax, const char *s, size_t slen)
{
	char *dest_base;
	size_t dest_avail;
	uint64_t overlap_guard;

	if (d == NULL || s == NULL) {
		pr_err("%s: invlaid src or dest buffer", __func__);
		return NULL;
	}

	if (dmax == 0 || slen == 0) {
		pr_err("%s: invlaid length of src or dest buffer", __func__);
		return NULL;
	}

	if (d == s)
		return d;

	overlap_guard = (uint64_t)((d > s) ? (d - s - 1) : (s - d - 1));

	dest_base = d;
	dest_avail = dmax;

	while (dest_avail > 0) {
		if (overlap_guard == 0) {
			pr_err("%s: overlap happened.", __func__);
			*(--d) = '\0';
			return NULL;
		}

		if (slen == 0) {
			*d = '\0';
			return dest_base;
		}

		*d = *s;
		if (*d == '\0')
			return dest_base;

		d++;
		s++;
		slen--;
		dest_avail--;
		overlap_guard--;
	}

	pr_err("%s: dest buffer has no enough space.", __func__);

	/*
	 * to avoid a string that is not
	 * null-terminated in dest buffer
	 */
	dest_base[dmax - 1] = '\0';
	return NULL;
}
