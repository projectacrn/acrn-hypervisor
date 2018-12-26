/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

/*
 * Convert a string to a long integer - decimal support only.
 */
int64_t strtol_deci(const char *nptr)
{
	const char *s = nptr;
	char c;
	uint64_t acc, cutoff, cutlim;
	int32_t neg = 0, any;
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
