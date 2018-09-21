/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <errno.h>
#include <regex.h>
#include <stdarg.h>
#include <stdlib.h>
#include "log_sys.h"
#include "fsutils.h"

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
char *strrstr(const char *s, const char *substr)
{
	const char *found;
	const char *p = s;

	while ((found = strstr(p, substr)))
		p = found + 1;

	if (p != s)
		return (char *)(p - 1);

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
	size_t len;
	char *end;

	len = strlen(str);
	if (len > 0) {
		end = str + strlen(str) - 1;
		while (*end == ' ' && end >= str) {
			*end = 0;
			end--;
		}
	}

	return str;
}

char *strtrim(char *str)
{
	if (str) {
		strtrimr(str);
		return strtriml(str);
	}

	return NULL;
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

static int reg_match(const char *str, const char *pattern,
		char *matched_sub, size_t matched_space,
		size_t *end_off)
{
	int err;
	regex_t reg;
	char err_msg[128];
	regmatch_t pm[1];
	size_t matched_len;

	LOGD("reg: %s\n", pattern);
	err = regcomp(&reg, pattern, REG_EXTENDED);
	if (err) {
		regerror(err, &reg, err_msg, sizeof(err_msg));
		LOGE("failed to regcomp - %s\n", err_msg);
		return -1;
	}

	err = regexec(&reg, str, sizeof(pm)/sizeof(regmatch_t), pm, 0);
	regfree(&reg);
	if (err == REG_NOMATCH) {
		LOGE("failed to match with reg (%s) str (%s)\n", pattern, str);
		return -1;
	}
	if (pm[0].rm_so == -1)
		return -1;

	*end_off = pm[0].rm_eo;
	if (matched_space == 0 || matched_sub == NULL)
		/* get offset only */
		return 0;

	matched_len = pm[0].rm_eo - pm[0].rm_so;
	*(char *)mempcpy(matched_sub, str + pm[0].rm_so,
			 MIN(matched_len, matched_space - 1)) = '\0';
	return 0;
}

static char *exp_end(const char *fmt, size_t flen, const char *exp_s)
{
	/* supports %[regex..] and %*[regex..] */
	int flag = 0;
	const char *p;

	if (exp_s < fmt || exp_s >= fmt + flen)
		return NULL;

	for (p = exp_s; p < fmt + flen; p++) {
		if (*p == '[')
			flag++;

		if (*p == ']') {
			flag--;
			if (flag == 0)
				return (char *)(p + 1);
			else if (flag < 0)
				return NULL;
		}
	}
	return NULL;
}

static int exp2reg(const char *s, const char *e, int *ignore_flag, char **reg)
{
	const char *oreg_s;
	int flag_tmp;
	char *buf;

	if (memcmp(s, "%*[", 3) == 0) {
		flag_tmp = 1;
		oreg_s = s + 3;
	} else if (memcmp(s, "%[", 2) == 0) {
		flag_tmp = 0;
		oreg_s = s + 2;
	} else {
		LOGE("invalid exp - exp must start with \"%%[\" or \"%%*[\"\n");
		return -1;
	}

	if (oreg_s >= e) {
		LOGE("invalid exp - exp empty\n");
		return -1;
	}

	buf = malloc(e - oreg_s + 1);
	if (!buf) {
		LOGE("out-of-mem\n");
		return -1;
	}

	buf[0] = '^';
	memcpy(&buf[1], oreg_s, e - oreg_s - 1);
	buf[e - oreg_s] = '\0';

	*reg = buf;
	*ignore_flag = flag_tmp;
	return 0;
}

int str_split_ere(const char *str, size_t slen,
		const char *fmt, size_t flen, ...)
{
	va_list v;
	char *_str, *_fmt;
	const char *exp_s, *exp_e;
	int ignore_flag;
	char *reg;
	size_t str_off = 0;
	size_t off;
	char *sreq;
	size_t sreqsize;
	int ret = 0;

	if (!str || !slen || !fmt || !flen)
		return ret;

	_str = strndup(str, slen);
	if (!_str)
		return ret;
	_fmt = strndup(fmt, flen);
	if (!_fmt) {
		free(_str);
		return ret;
	}

	va_start(v, flen);
	/* supports %[regex..] and %*[regex..] */
	exp_s = fmt;
	while (str_off < flen && *exp_s) {
		exp_e = exp_end(fmt, flen, exp_s);
		if (!exp_e) {
			LOGE("invalid exp - failed to find the end of exp\n");
			goto out;
		}

		if (exp2reg(exp_s, exp_e, &ignore_flag, &reg) == -1) {
			LOGE("failed to translate exp to reg\n");
			goto out;
		}

		if (ignore_flag == 1) {
			sreq = NULL;
			sreqsize = 0;
		} else {
			sreq = va_arg(v, char *);
			sreqsize = va_arg(v, size_t);
		}

		if (reg_match(str + str_off, reg, sreq, sreqsize, &off) == -1) {
			LOGE("failed to match reg\n");
			free(reg);
			goto out;
		} else {
			if (ignore_flag == 0)
				ret++;
		}

		exp_s = exp_e;
		str_off += off;
		free(reg);
	}

out:
	va_end(v);
	free(_str);
	free(_fmt);
	return ret;
}
