/*
 * Copyright (C)2021-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _LOG_H_
#define _LOG_H_
#include <time.h>

extern FILE *log_fd;
static inline void output_timestamp(void)
{
	struct tm *t;
	time_t tt;

	time(&tt);
	t = localtime(&tt);
	fprintf(log_fd, "[%4d-%02d-%02d %02d:%02d:%02d]", t->tm_year + 1900, \
		t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
}
#define LOG_PRINTF(format, args...) \
do { output_timestamp(); \
	fprintf(log_fd, format, args); \
	fflush(log_fd); } while (0)

#define LOG_WRITE(args) \
do { output_timestamp(); \
	fwrite(args, 1, sizeof(args), log_fd); \
	fflush(log_fd); } while (0)

static inline bool open_log(const char *path)
{
	bool ret = false;

	log_fd = fopen("/var/log/life_mngr.log", "a+");
	if (log_fd != NULL)
		ret = true;
	return ret;
}
static inline void close_log(void)
{
	if (log_fd != NULL)
		fclose(log_fd);
}
#endif
