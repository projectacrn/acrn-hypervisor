/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "dm_string.h"

int
dm_strtol(const char *s, char **end, unsigned int base, long *val)
{
	if (!s)
		return -1;

	*val = strtol(s, end, base);
	if ((end && *end == s) || errno == ERANGE)
		return -1;
	return 0;
}

int
dm_strtoi(const char *s, char **end, unsigned int base, int *val)
{
	long l_val;
	int ret;

	l_val = 0;
	ret = dm_strtol(s, end, base, &l_val);
	if (ret == 0)
		*val = (int)l_val;
	return ret;
}

int
dm_strtoul(const char *s, char **end, unsigned int base, unsigned long *val)
{
	if (!s)
		return -1;

	*val = strtoul(s, end, base);
	if ((end && *end == s) || errno == ERANGE)
		return -1;
	return 0;
}

int
dm_strtoui(const char *s, char **end, unsigned int base, unsigned int *val)
{
	unsigned long l_val;
	int ret;

	l_val = 0;
	ret = dm_strtoul(s, end, base, &l_val);
	if (ret == 0)
		*val = (unsigned int)l_val;
	return ret;
}
