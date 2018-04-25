/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <errno.h>

/**
 * Get the length of line.
 *
 * @param str Start address of line.
 *
 * @return the length of line if successful, or -1 if not.
 *	   This function return length of string if string doesn't contain \n.
 */
int strlinelen(char *str)
{
	char *tag;

	if (!str)
		return -1;

	tag = strchr(str, '\n');
	if (tag)
		return tag - str + 1;

	return strlen(str);
}

/**
 * Find the last occurrence of the substring str in the string s.
 *
 * @param s Range of search.
 * @param substr String to be found.
 *
 * @return a pointer to the beginning of the substring,
 *	   or NULL if the substring is not found.
 */
char *strrstr(char *s, char *substr)
{
	char *found;
	char *p = s;

	while ((found = strstr(p, substr)))
		p = found + 1;

	if (p != s)
		return p - 1;

	return NULL;
}

char *next_line(char *buf)
{
	char *p;

	p  = strchr(buf, '\n');
	/* if meet end of buf, the return value is also NULL */
	if (p)
		return p + 1;

	return NULL;
}

static char *strtriml(char *str)
{
	char *p = str;

	while (*p == ' ')
		p++;
	return memmove(str, p, strlen(p) + 1);
}

static char *strtrimr(char *str)
{
	char *end = str + strlen(str) - 1;

	while (*end == ' ' && end >= str) {
		*end = 0;
		end--;
	}

	return str;
}

char *strtrim(char *str)
{
	strtrimr(str);
	return strtriml(str);
}

int strcnt(char *str, char c)
{
	int cnt = 0;
	char *p = str;
	char *found;

	if (!str)
		return -EINVAL;

	while ((found = strchr(p, c))) {
		p = found + 1;
		cnt++;
	}

	return cnt;
}
