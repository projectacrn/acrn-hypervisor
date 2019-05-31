/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LOG_H__
#define __LOG_H__

#include "types.h"

/* Logging severity levels */
#define LOG_ERROR		1U
#define LOG_WARNING		2U
#define LOG_NOTICE		3U
#define LOG_INFO			4U
#define LOG_DEBUG		5U

#define DEFAULT_LOG_LEVEL         4
#define MAX_ONE_LOG_SIZE     256

struct logger_ops {
	const char *name;
	bool (*is_enabled)(void);
	uint8_t (*get_log_level)(void);
	int (*init)(bool enable, uint8_t log_level);
	void (*deinit)(void);
	void (*output)(const char *fmt, va_list args);
};

int init_logger_setting(const char *opt);
void deinit_loggers(void);
void output_log(uint8_t level, const char *fmt, ...);

/*
 * Put all logger instances' addresses into one section named logger_dev_ops
 * so that DM could enumerate and initialize each of them.
 */
#define DECLARE_LOGGER_SECTION()    SET_DECLARE(logger_dev_ops, struct logger_ops)
#define DEFINE_LOGGER_DEVICE(x)    DATA_SET(logger_dev_ops, x)
#define FOR_EACH_LOGGER(pp_logger)    SET_FOREACH(pp_logger, logger_dev_ops)


#ifndef pr_prefix
#define pr_prefix
#endif

#define pr_err(...) output_log(LOG_ERROR, pr_prefix __VA_ARGS__)
#define pr_warn(...) output_log(LOG_WARNING, pr_prefix __VA_ARGS__)
#define pr_notice(...) output_log(LOG_NOTICE, pr_prefix __VA_ARGS__)
#define pr_info(...) output_log(LOG_INFO, pr_prefix __VA_ARGS__)
#define pr_dbg(...) output_log(LOG_DEBUG, pr_prefix __VA_ARGS__)

#endif /* __LOG_H__ */
