/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __STRUTILS_H__
#define __STRUTILS_H__

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int strlinelen(char *str);
char *strrstr(const char *s, const char *str);
char *next_line(char *buf);
char *strtrim(char *str);
int strcnt(char *str, char c);
int str_split_ere(const char *str, size_t slen,
		const char *fmt, size_t flen, ...);
#endif
