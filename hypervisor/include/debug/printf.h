/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PRINTF_H
#define PRINTF_H

#ifdef HV_DEBUG
/** The well known printf() function.
 *
 *  Formats a string and writes it to the console output.
 *
 *  @param fmt A pointer to the NUL terminated format string.
 *
 *  @return The number of characters actually written or a negative
 *          number if an error occurred.
 */

int printf(const char *fmt, ...);

/** The well known vprintf() function.
 *
 *  Formats a string and writes it to the console output.
 *
 *  @param fmt A pointer to the NUL terminated format string.
 *  @param args The variable long argument list as va_list.
 *  @return The number of characters actually written or a negative
 *          number if an error occurred.
 */

int vprintf(const char *fmt, va_list args);

#else /* HV_DEBUG */

static inline int printf(__unused const char *fmt, ...)
{
	return 0;
}

static inline int vprintf(__unused const char *fmt, __unused va_list args)
{
	return 0;
}

#endif /* HV_DEBUG */

#endif /* PRINTF_H */
