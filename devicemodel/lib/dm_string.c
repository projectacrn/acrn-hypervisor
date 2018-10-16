/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dm_string.h"

int
dm_strtol(char *s, char **end, unsigned int base, long *val)
{
	if (!s)
		goto err;

	*val = strtol(s, end, base);
	if (*end == s) {
		printf("ERROR! nothing covert for: %s!\n", s);
		goto err;
	}
	return 0;

err:
	return -1;
}

int
dm_strtoi(char *s, char **end, unsigned int base, int *val)
{
	long l_val;
	int ret;

	l_val = 0;
	ret = dm_strtol(s, end, base, &l_val);
	*val = (int)l_val;
	return ret;
}

int
dm_strtoul(char *s, char **end, unsigned int base, unsigned long *val)
{
	if (!s)
		goto err;

	*val = strtoul(s, end, base);
	if (*end == s) {
		printf("ERROR! nothing covert for: %s!\n", s);
		goto err;
	}
	return 0;

err:
	return -1;
}

int
dm_strtoui(char *s, char **end, unsigned int base, unsigned int *val)
{
	unsigned long l_val;
	int ret;

	l_val = 0;
	ret = dm_strtoul(s, end, base, &l_val);
	*val = (unsigned int)l_val;
	return ret;
}
