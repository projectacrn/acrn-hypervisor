/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>

#include "log.h"


DECLARE_LOGGER_SECTION();

/*
 * --logger_setting: console,level=4;disk,level=4;kmsg,level=3
 * the setting param is from acrn-dm input, will be parsed here
 */
void init_logger_setting(const char *opt)
{

}

void output_log(uint8_t level, const char *fmt, ...)
{
	va_list args;
	struct logger_ops **pp_logger, *logger;

	/* check each logger flag and level, to output */
	FOR_EACH_LOGGER(pp_logger) {
		logger = *pp_logger;
		if (logger->is_enabled() && (level <= logger->get_log_level()) && (logger->output)) {
			va_start(args, fmt);
			logger->output(fmt, args);
			va_end(args);
		}
	}
}

/* console setting and its API interface */
static uint8_t console_log_level = DEFAULT_LOG_LEVEL;
static bool console_enabled = true;

static bool is_console_enabled(void)
{
	return console_enabled;
}

static uint8_t get_console_log_level(void)
{
	return console_log_level;
}

static int init_console_setting(bool enable, uint8_t log_level)
{
	console_enabled = enable;
	console_log_level = log_level;

	return 0;
}

static void write_to_console(const char *fmt, va_list args)
{
	/* if no need add other info, just output */
	vprintf(fmt, args);
}

struct logger_ops logger_console = {
	.name = "console",
	.is_enabled = is_console_enabled,
	.get_log_level = get_console_log_level,
	.init = init_console_setting,
	.output = write_to_console,
};


DEFINE_LOGGER_DEVICE(logger_console);
