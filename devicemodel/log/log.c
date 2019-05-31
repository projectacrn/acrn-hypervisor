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

#include "dm_string.h"
#include "log.h"


DECLARE_LOGGER_SECTION();

/*
 * --logger_setting: console,level=4;disk,level=4;kmsg,level=3
 * the setting param is from acrn-dm input, will be parsed here
 */
int init_logger_setting(const char *opt)
{
	char *orig, *str, *elem, *name, *level;
	uint32_t lvl_val;
	int error = 0;
	struct logger_ops **pp_logger, *plogger;

	orig = str = strdup(opt);
	if (!str) {
		fprintf(stderr, "%s: strdup returns NULL\n", __func__);
		return -1;
	}

	/* param example: --logger_setting console,level=4;kmsg,level=3 */
	for (elem = strsep(&str, ";"); elem != NULL; elem = strsep(&str, ";")) {
		name = strsep(&elem, ",");
		level = elem;

		if ((strncmp(level, "level=", 6) != 0) || (dm_strtoui(level + 6, &level, 10, &lvl_val))) {
			fprintf(stderr, "logger setting param error: %s, please check!\n", elem);
			error = -1;
			break;
		}

		printf("logger: name=%s, level=%d\n", name, lvl_val);

		plogger = NULL;
		FOR_EACH_LOGGER(pp_logger) {
			plogger = *pp_logger;
			if (strcmp(name, plogger->name) == 0) {
				if (plogger->init)
					plogger->init(true, (uint8_t)lvl_val);

				break;
			}
		}

		if (plogger == NULL) {
			fprintf(stderr, "there is no logger: %s found in DM, please check!\n", name);
			error = -1;
			break;
		}
	}

	free(orig);
	return error;
}

void deinit_loggers(void)
{
	struct logger_ops **pp_logger, *plogger;

	FOR_EACH_LOGGER(pp_logger) {
		plogger = *pp_logger;
		if (plogger->deinit)
			plogger->deinit();
	}
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

static struct logger_ops logger_console = {
	.name = "console",
	.is_enabled = is_console_enabled,
	.get_log_level = get_console_log_level,
	.init = init_console_setting,
	.output = write_to_console,
};


DEFINE_LOGGER_DEVICE(logger_console);
