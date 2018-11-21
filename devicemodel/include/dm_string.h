/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _DM_STRING_H_
#define _DM_STRING_H_

/**
 * @brief Convert string to a long integer.
 *
 * @param s Pointer to original string.
 * @param end Pointer to end string.
 * @param base Base: 8, 10, 16...
 * @param val Long integer convert from string.
 *
 * @retval -1 error.
 * @retval 0  no error.
 */

int dm_strtol(const char *s, char **end, unsigned int base, long *val);

/**
 * @brief Convert string to an integer.
 *
 * @param s Pointer to original string.
 * @param end Pointer to end string.
 * @param base Base: 8, 10, 16...
 * @param val Integer convert from string.
 *
 * @retval -1 error.
 * @retval 0  no error.
 */

int dm_strtoi(const char *s, char **end, unsigned int base, int *val);

/**
 * @brief Convert string to an unsigned long integer.
 *
 * @param s Pointer to original string.
 * @param end Pointer to end string.
 * @param base Base: 8, 10, 16...
 * @param val Unsigned long integer convert from string.
 *
 * @retval -1 error.
 * @retval 0  no error.
 */

int dm_strtoul(const char *s, char **end, unsigned int base, unsigned long *val);

/**
 * @brief Convert string to an unsigned integer.
 *
 * @param s Pointer to original string.
 * @param end Pointer to end string after strtol.
 * @param base Base: 8, 10, 16...
 * @param val Unsigned integer convert from string.
 *
 * @retval -1 error.
 * @retval 0  no error.
 */

int dm_strtoui(const char *s, char **end, unsigned int base, unsigned int *val);

#endif
