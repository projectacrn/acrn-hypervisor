/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#define ULONG_MAX       ((uint64_t)(~0UL))              /* 0xFFFFFFFF */
#define LONG_MAX        (ULONG_MAX >> 1U)        /* 0x7FFFFFFF */
#define LONG_MIN        (~LONG_MAX)             /* 0x80000000 */

static inline bool is_space(char c)
{
	return ((c == ' ') || (c == '\t'));
}

static inline char hex_digit_value (char ch)
{
	char c;
	if ('0' <= ch && ch <= '9') {
		c = ch - '0';
	} else if ('a' <= ch && ch <= 'f') {
		c = ch - 'a' + 10;
	} else if ('A' <= ch && ch <= 'F') {
		c = ch - 'A' + 10;
	} else {
		c = -1;
	}
	return c;
}

/*
 * Convert a string to a long integer - decimal support only.
 */
long strtol_deci(const char *nptr)
{
	const char *s = nptr;
	char c;
	uint64_t acc, cutoff, cutlim;
	int neg = 0, any;
	uint64_t base = 10UL;

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 */
	do {
		c = *s;
		s++;
	} while (is_space(c));

	if (c == '-') {
		neg = 1;
		c = *s;
		s++;
	} else if (c == '+') {
		c = *s;
		s++;
	} else {
		/* No sign character. */
	}

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = (neg != 0) ? LONG_MIN : LONG_MAX;
	cutlim = cutoff % base;
	cutoff /= base;
	acc = 0UL;
	any = 0;

	while ((c >= '0') && (c <= '9')) {
		c -= '0';
		if ((acc > cutoff) ||
			((acc == cutoff) && ((uint64_t)c > cutlim))) {
			any = -1;
			break;
		} else {
			acc *= base;
			acc += (uint64_t)c;
		}

		c = *s;
		s++;
	}

	if (any < 0) {
		acc = (neg != 0) ? LONG_MIN : LONG_MAX;
	} else if (neg != 0) {
		acc = ~acc + 1UL;
	} else {
		/* There is no overflow and no leading '-' exists. In such case
		 * acc already holds the right number. No action required. */
	}
	return (long)acc;
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
	int any;

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

int atoi(const char *str)
{
	return (int)strtol_deci(str);
}

char *strchr(char *s_arg, char ch)
{
	char *s = s_arg;
	while ((*s != '\0') && (*s != ch)) {
		++s;
	}

	return ((*s) != '\0') ? s : NULL;
}

/**
 *strcpy_s
 *
 * description:
 *    This function copies the string pointed to by s to a buffer
 *    pointed by d.
 *
 * input:
 *    d        pointer to dest buffer.
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
char *strcpy_s(char *d_arg, size_t dmax, const char *s_arg)
{
	char *d = d_arg;
	const char *s = s_arg;
	char *dest_base;
	size_t dest_avail;
	uint64_t overlap_guard;

	if ((s == NULL) || (d == NULL) || (dmax == 0U)) {
		pr_err("%s: invalid src, dest buffer or length.", __func__);
		return NULL;
	}

	if (s == d) {
		return d;
	}

	overlap_guard = (uint64_t)((d > s) ? (d - s - 1) : (s - d - 1));

	dest_avail = dmax;
	dest_base = d;

	while (dest_avail > 0U) {
		if (overlap_guard == 0U) {
			pr_err("%s: overlap happened.", __func__);
			d--;
			*d = '\0';
			return NULL;
		}

		*d = *s;
		if (*d == '\0') {
			return dest_base;
		}

		d++;
		s++;
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
char *strncpy_s(char *d_arg, size_t dmax, const char *s_arg, size_t slen_arg)
{
	const char *s = s_arg;
	char *d = d_arg;
	char *dest_base;
	size_t dest_avail;
	uint64_t overlap_guard;
	size_t slen = slen_arg;

	if ((d == NULL) || (s == NULL)) {
		pr_err("%s: invlaid src or dest buffer", __func__);
		return NULL;
	}

	if ((dmax == 0U) || (slen == 0U)) {
		pr_err("%s: invlaid length of src or dest buffer", __func__);
		return NULL;
	}

	if (d == s) {
		return d;
	}

	overlap_guard = (uint64_t)((d > s) ? (d - s - 1) : (s - d - 1));

	dest_base = d;
	dest_avail = dmax;

	while (dest_avail > 0U) {
		if (overlap_guard == 0U) {
			pr_err("%s: overlap happened.", __func__);
			d--;
			*d = '\0';
			return NULL;
		}

		if (slen == 0U) {
			*d = '\0';
			return dest_base;
		}

		*d = *s;
		if (*d == '\0') {
			return dest_base;
		}

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
	size_t maxlen = maxlen_arg;
	size_t count;

	if (str == NULL) {
		return 0;
	}

	count = 0U;
	while ((*str) != '\0') {
		if (maxlen == 0U) {
			break;
		}

		count++;
		maxlen--;
		str++;
	}

	return count;
}

int strcmp(const char *s1_arg, const char *s2_arg)
{
	const char *s1 = s1_arg;
	const char *s2 = s2_arg;
	while (((*s1) != '\0') && ((*s2) != '\0') && ((*s1) == (*s2))) {
		s1++;
		s2++;
	}

	return *s1 - *s2;
}

int strncmp(const char *s1_arg, const char *s2_arg, size_t n_arg)
{
	const char *s1 = s1_arg;
	const char *s2 = s2_arg;
	size_t n = n_arg;
	while (((n - 1) != 0U) && ((*s1) != '\0') && ((*s2) != '\0')
		&& ((*s1) == (*s2))) {
		s1++;
		s2++;
		n--;
	}

	return *s1 - *s2;
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
	const char *pstr;

	if ((str1 == NULL) || (str2 == NULL)) {
		return NULL;
	}

	if ((maxlen1 == 0U) || (maxlen2 == 0U)) {
		return NULL;
	}

	len1 = strnlen_s(str1, maxlen1);
	len2 = strnlen_s(str2, maxlen2);

	if (len1 < len2) {
		return NULL;
	}

	/* return str1 if str2 equals to str1 or str2 points to a string with zero length*/
	if ((str1 == str2) || (len2 == 0U)) {
		return (char *)str1;
	}

	pstr = str1;
	while (len1 >= len2) {
		for (i = 0U; i < len2; i++) {
			if (pstr[i] != str2[i]) {
				break;
			}
		}
		if (i == len2) {
			return (char *)pstr;
		}
		pstr++;
		len1--;
	}

	/*
	 * substring was not found, return NULL
	 */
	return NULL;
}
