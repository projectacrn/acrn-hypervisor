/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hv_lib.h>

static int do_udiv32(uint32_t dividend, uint32_t divisor,
	struct udiv_result *res)
{

	uint32_t mask;
	/* dividend is always greater than or equal to the divisor. Neither
	 * divisor nor dividend are 0. Thus: * clz(dividend) and clz(divisor)
	 * are valid * clz(dividend)<=clz(divisor)
	 */

	mask = (uint32_t)(clz(divisor) - clz(dividend));
	/* align divisor and dividend */
	divisor <<= mask;
	mask = 1U << mask;
	/* division loop */
	do {
		if (dividend >= divisor) {
			dividend -= divisor;
			res->q.dwords.low |= mask;
		}
		divisor >>= 1U;
		mask >>= 1U;
	} while ((mask != 0U) && (dividend != 0U));
	/* dividend now contains the reminder */
	res->r.dwords.low = dividend;
	return 0;
}

int udiv32(uint32_t dividend, uint32_t divisor, struct udiv_result *res)
{

	/* initialize the result */
	res->q.dwords.low = 0U;
	res->r.dwords.low = 0U;
	/* test for "division by 0" condition */
	if (divisor == 0U) {
		res->q.dwords.low = 0xffffffffU;
		return 1;
	}
	/* trivial case: divisor==dividend */
	if (divisor == dividend) {
		res->q.dwords.low = 1U;
		return 0;
	}
	/* trivial case: divisor>dividend */
	if (divisor > dividend) {
		res->r.dwords.low = dividend;
		return 0;
	}
	/* now that the trivial cases are eliminated we can call the generic
	 * function.
	 */
	return do_udiv32(dividend, divisor, res);
}

int udiv64(uint64_t dividend, uint64_t divisor, struct udiv_result *res)
{

	uint64_t mask;
	uint64_t bits;

	/* initialize the result */
	res->q.qword = 0UL;
	res->r.qword = 0UL;
	/* test for "division by 0" condition */
	if (divisor == 0UL) {
		res->q.qword = 0xffffffffffffffffUL;
		return -1;
	}
	/* trivial case: divisor==dividend */
	if (divisor == dividend) {
		res->q.qword = 1UL;
		return 0;
	}
	/* trivial case: divisor>dividend */
	if (divisor > dividend) {
		res->r.qword = dividend;
		return 0;
	}
	/* simplified case: only 32 bit operands Note that the preconditions
	 * for do_udiv32() are fulfilled, since the tests were made above.
	 */
	if (((divisor >> 32UL) == 0UL) && ((dividend >> 32UL) == 0UL)) {
		return do_udiv32((uint32_t) dividend, (uint32_t) divisor, res);
	}

	/* dividend is always greater than or equal to the divisor. Neither
	 * divisor nor dividend are 0. Thus: * clz(dividend) and clz(divisor)
	 * are valid * clz(dividend)<=clz(divisor)
	 */

	/* align divisor and dividend. */
	bits = (uint64_t)(clz64(divisor) - clz64(dividend));
	divisor <<= bits;
	mask = 1UL << bits;
	/* division loop */
	do {
		if (dividend >= divisor) {
			dividend -= divisor;
			res->q.qword |= mask;
		}
		divisor >>= 1UL;
		mask >>= 1UL;
	} while ((bits-- != 0UL) && (dividend != 0UL));

	res->r.qword = dividend;
	return 0;
}
