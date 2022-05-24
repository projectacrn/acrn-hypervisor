/*
 * Copyright (C) 2018-2020 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <rtl.h>
#include <logmsg.h>

static inline char hex_digit_value(char ch)
{
	char c;
	if (('0' <= ch) && (ch <= '9')) {
		c = ch - '0';
	} else if (('a' <= ch) && (ch <= 'f')) {
		c = ch - 'a' + 10;
	} else if (('A' <= ch) && (ch <= 'F')) {
		c = ch - 'A' + 10;
	} else {
		c = -1;
	}
	return c;
}

/*
 * Convert a string to an uint64_t integer - hexadecimal support only.
 */
uint64_t strtoul_hex(const char *nptr)
{
	const char *s = nptr;
	char c, digit;
	uint64_t acc, cutoff, cutlim;
	uint64_t base = 16UL;
	int32_t any;

	/*
	 * See strtol for comments as to the logic used.
	 */
	do {
		c = *s;
		s++;
	} while (is_space(c));

	if ((c == '0') && ((*s == 'x') || (*s == 'X'))) {
		c = s[1];
		s += 2;
	}

	cutoff = ULONG_MAX / base;
	cutlim = ULONG_MAX % base;
	acc = 0UL;
	any = 0;
	digit = hex_digit_value(c);
	while (digit >= 0) {
		if ((acc > cutoff) || ((acc == cutoff) && ((uint64_t)digit > cutlim))) {
			any = -1;
			break;
		} else {
			acc *= base;
			acc += (uint64_t)digit;
		}

		c = *s;
		s++;
		digit = hex_digit_value(c);
	}

	if (any < 0) {
		acc = ULONG_MAX;
	}
	return acc;
}

char *strchr(char *s_arg, char ch)
{
	char *s = s_arg;
	while ((*s != '\0') && (*s != ch)) {
		++s;
	}

	return ((*s) != '\0') ? s : NULL;
}

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
 *    0		if source string is copied successfully;
 *    -1	if there is a runtime-constraint violation.
 *
 * notes:
 *    1) dmax shall not be 0.
 *    2) both d and s shall not be null pointers.
 *    3) Copying shall not take place between objects that overlap.
 *    4) If slen is not less than dmax, then dmax shall be more than strnlen_s(s, dmax).
 *    5) d[0] shall be set to '\0' if there is a runtime-constraint violation.
 */
int32_t strncpy_s(char *d, size_t dmax, const char *s, size_t slen)
{
	char *dest = d;
	int32_t ret = -1;
	size_t len = strnlen_s(s, dmax);

	if ((slen < dmax) || (dmax > len)) {
		ret = memcpy_s(d, dmax, s, len);
	}

	if (ret == 0) {
		*(dest + len) = '\0';
	} else {
		if ((d != NULL) && (dmax > 0U)) {
			*dest = '\0';
		}
	}

	return ret;
}

/**
 *
 *    strnlen_s
 *
 * description:
 *    The function calculates the length of the string pointed
 *    to by str.
 *
 *
 * input:
 *    str      pointer to the null-terminated string to be examined.
 *
 *    dmax      maximum number of characer to examine.
 *
 * return value:
 *    string length, excluding the null character.
 *    will return 0 if str is null.
 */
size_t strnlen_s(const char *str_arg, size_t maxlen_arg)
{
	const char *str = str_arg;
	size_t count = 0U;

	if (str != NULL) {
		size_t maxlen = maxlen_arg;
		while ((*str) != '\0') {
			if (maxlen == 0U) {
				break;
			}

			count++;
			maxlen--;
			str++;
		}
	}

	return count;
}

int32_t strcmp(const char *s1_arg, const char *s2_arg)
{
	const char *str1 = s1_arg;
	const char *str2 = s2_arg;

	while (((*str1) != '\0') && ((*str2) != '\0') && ((*str1) == (*str2))) {
		str1++;
		str2++;
	}

	return *str1 - *str2;
}

/**
 * @pre n_arg > 0
 */
int32_t strncmp(const char *s1_arg, const char *s2_arg, size_t n_arg)
{
	const char *str1 = s1_arg;
	const char *str2 = s2_arg;
	size_t n = n_arg;
	int32_t ret = 0;

	if (n > 0U) {
		while (((n - 1) != 0U) && ((*str1) != '\0') && ((*str2) != '\0') && ((*str1) == (*str2))) {
			str1++;
			str2++;
			n--;
		}
		ret = (int32_t) (*str1 - *str2);
	}

	return ret;
}

/*
 * strstr_s
 *
 * description:
 *    Search str2 in str1
 *
 * input:
 *    str1      pointer to string to be searched for the substring.
 *
 *    maxlen1   maximum length of str1.
 *
 *    str2      pointer to the sub-string.
 *
 *    maxlen2   maximum length of str2.
 *
 * return value:
 *     Pointer to the first occurrence of str2 in str1,
 *     or return null if not found.
 */
char *strstr_s(const char *str1, size_t maxlen1, const char *str2, size_t maxlen2)
{
	size_t len1, len2;
	size_t i;
	const char *pstr, *pret;

	if ((str1 == NULL) || (str2 == NULL)) {
		pret = NULL;
	} else if ((maxlen1 == 0U) || (maxlen2 == 0U)) {
		pret = NULL;
	} else {
		len1 = strnlen_s(str1, maxlen1);
		len2 = strnlen_s(str2, maxlen2);

		if (len1 < len2) {
			pret = NULL;
		} else if ((str1 == str2) || (len2 == 0U)) {
			/* return str1 if str2 equals to str1 or str2 points to a string with zero length*/
			pret = str1;
		} else {
			pret = NULL;
			pstr = str1;
			while (len1 >= len2) {
				for (i = 0U; i < len2; i++) {
					if (pstr[i] != str2[i]) {
						break;
					}
				}
				if (i == len2) {
					pret = pstr;
					break;
				}
				pstr++;
				len1--;
			}
		}
	}

	return (char *)pret;
}

/*
 * strncat_s
 *
 * description:
 *    append src string to the end of dest string
 *
 * input:
 *    dest      pointer to the string to be appended.
 *
 *    dmax      maximum length of dest buffer including the NULL char.
 *
 *    src       pointer to the string that to be concatenated to string dest.
 *
 *    slen      maximum characters to append.
 *
 * return value:
 *     0 for success, -1 for failure.
 */
int32_t strncat_s(char *dest, size_t dmax, const char *src, size_t slen)
{
	int32_t ret = -1;
	size_t len_d, len_s;
	char *d = dest, *start;

	len_d = strnlen_s(dest, dmax);
	len_s = strnlen_s(src, slen);
	start = dest + len_d;

	if ((dest != NULL) && (src != NULL) && (dmax > (len_d + len_s))
			&& ((dest > (src + len_s)) || (src > (dest + len_d)))) {
		(void)memcpy_s(start, (dmax - len_d), src, len_s);
		*(start + len_s) = '\0';
		ret = 0;
	} else {
		if (dest != NULL) {
			*d = '\0';	/* set dest[0] to NULL char on runtime-constraint violation */
		}
	}
	return ret;
}
