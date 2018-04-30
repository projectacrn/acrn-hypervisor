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

extern void *malloc(UINTN size);
extern void free(void *buf);
extern void *calloc(UINTN nmemb, UINTN size);

extern EFI_STATUS emalloc(UINTN, UINTN, EFI_PHYSICAL_ADDRESS *);
extern EFI_STATUS __emalloc(UINTN, UINTN, EFI_PHYSICAL_ADDRESS *, EFI_MEMORY_TYPE);
extern EFI_STATUS emalloc_for_low_mem(EFI_PHYSICAL_ADDRESS *addr, UINTN size);
extern void efree(EFI_PHYSICAL_ADDRESS, UINTN);

static inline void memset(void *dstv, char ch, UINTN size)
{
	char *dst = dstv;
	int i;

	for (i = 0; i < size; i++)
		dst[i] = ch;
}

static inline void memcpy(char *dst, const char *src, UINTN size)
{
	int i;

	for (i = 0; i < size; i++)
		*dst++ = *src++;
}

static inline int strlen(const char *str)
{
	int len;

	len = 0;
	while (*str++)
		len++;

	return len;
}

static inline char *strstr(const char *haystack, const char *needle)
{
	const char *p;
	const char *word = NULL;
	int len = strlen(needle);

	if (!len)
		return NULL;

	p = haystack;
	while (*p) {
		word = p;
		if (!strncmpa((CHAR8 *)p, (CHAR8 *)needle, len))
			break;
		p++;
		word = NULL;
	}

	return (char *)word;
}

static inline char *strdup(const char *src)
{
	int len;
	char *dst;

	len = strlen(src);
	dst = malloc(len + 1);
	if (dst)
		memcpy(dst, src, len + 1);
	return dst;
}

static inline CHAR16 *strstr_16(CHAR16 *haystack, CHAR16 *needle)
{
	CHAR16 *p;
	CHAR16 *word = NULL;
	UINTN len = StrLen(needle);

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

static inline CHAR16 *ch8_2_ch16(char *str8)
{
	UINTN len, i;
	CHAR16 *str16;

	len = strlen(str8);
	str16 = AllocatePool((len + 1) * sizeof(CHAR16));

	for (i = 0; i < len; i++)
		str16[i] = str8[i];

	str16[len] = 0;

	return str16;
}

#endif /* __STDLIB_H__ */
