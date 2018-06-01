/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ASSERT_H
#define ASSERT_H
#define __STDC_99__	199901L

/* Force a compilation error if condition is false */
#if __STDC_VERSION__ == __STDC_99__
#define _Static_assert(expr, error)	\
	extern char Static_Error[(expr) ? 1 : -1]
#endif

#ifdef HV_DEBUG
void __assert(uint32_t line, const char *file, char *txt);

#define ASSERT(x, ...) \
	if (!(x)) {\
		pr_fatal(__VA_ARGS__);\
		__assert(__LINE__, __FILE__, "fatal error");\
	}
#else
#define ASSERT(x, ...)	do { } while(0)
#endif

#endif /* ASSERT_H */
