/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __STRUTILS_H__
#define __STRUTILS_H__

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define s_not_expect(res, size) (res < 0 || (size_t)res >= size)

char *get_line(const char *str, size_t str_size,
		const char *area, size_t area_size,
		const char *search_from, size_t *len);
ssize_t strlinelen(const char *str, size_t size);
char *strrstr(const char *s, const char *str);
char *strtrim(char *str, size_t len);
int strcnt(char *str, char c);
char *strings_ind(char *strings, size_t size, int index, size_t *slen);
int str_split_ere(const char *str, size_t slen,
		const char *fmt, size_t flen, ...);
#endif
