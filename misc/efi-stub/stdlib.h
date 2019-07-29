/*
 * Copyright (c) 2011, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file contains some wrappers around the gnu-efi functions. As
 * we're not going through uefi_call_wrapper() directly, this allows
 * us to get some type-safety for function call arguments and for the
 * compiler to check that the number of function call arguments is
 * correct.
 *
 * It's also a good place to document the EFI interface.
 */



#ifndef __STDLIB_H__
#define __STDLIB_H__


static inline void memset(void *dstv, char ch, UINTN size)
{
	char *dst = dstv;
	int32_t i;

	for (i = 0; i < size; i++)
		dst[i] = ch;
}

static inline void memcpy(char *dst, const char *src, UINTN size)
{
	int32_t i;

	for (i = 0; i < size; i++)
		*dst++ = *src++;
}

static inline int32_t strlen(const char *str)
{
	int32_t len;

	len = 0;
	while (*str++)
		len++;

	return len;
}

static inline CHAR16 *strstr_16(CHAR16 *haystack, CHAR16 *needle, UINTN len)
{
	CHAR16 *p;
	CHAR16 *word = NULL;

	if (!len)
		return NULL;

	p = haystack;
	while (*p) {
		if (!StrnCmp(p, needle, len)) {
			word = p;
			break;
		}
		p++;
	}

	return (CHAR16*)word;
}

static inline char *ch16_2_ch8(CHAR16 *str16, UINTN len)
{
	UINTN i;
	char *str8;

	str8 = AllocatePool((len + 1) * sizeof(char));

	for (i = 0; i < len; i++)
		str8[i] = str16[i];

	str8[len] = 0;

	return str8;
}

#endif /* __STDLIB_H__ */
