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

#define KMSG_DEV_NODE "/dev/kmsg"
#define KMSG_PREFIX  "acrn_dm: "

#define KMSG_MAX_LEN    (MAX_ONE_LOG_SIZE + 64)
#define DEFAULT_KMSG_LEVEL    LOG_NOTICE

static int kmsg_fd = -1;

static uint8_t kmsg_log_level = DEFAULT_KMSG_LEVEL;
static bool kmsg_enabled = false;

static bool is_kmsg_enabled(void)
{
	return kmsg_enabled;
}

static uint8_t get_kmsg_log_level(void)
{
	return kmsg_log_level;
}

static int init_kmsg_logger(bool enable, uint8_t log_level)
{
	kmsg_enabled = enable;
	kmsg_log_level = log_level;

	/* usually this init should be called once in DM whole life */
	if (kmsg_fd > 0)
		return kmsg_fd;

	kmsg_fd = open(KMSG_DEV_NODE, O_RDWR | O_APPEND | O_NONBLOCK);
	if (kmsg_fd < 0) {
		kmsg_enabled = false;
		perror(KMSG_PREFIX"open kmsg dev failed");
	}

	return kmsg_fd;
}

static void deinit_kmsg(void)
{
	if (kmsg_fd > 0) {
		close(kmsg_fd);
		kmsg_fd = -1;

		kmsg_enabled = false;
	}
}

static void write_to_kmsg(const char *fmt, va_list args)
{
	char *buf;
	char kmsg_buf[KMSG_MAX_LEN] = KMSG_PREFIX;
	int len, write_cnt;

	if (kmsg_fd < 0)
		return;

	len = vasprintf(&buf, fmt, args);
	if (len < 0)
		return;
	strncpy(kmsg_buf + strlen(KMSG_PREFIX), buf, KMSG_MAX_LEN - strlen(KMSG_PREFIX));
	kmsg_buf[KMSG_MAX_LEN - 1] = '\0';
	free(buf);

	write_cnt = write(kmsg_fd, kmsg_buf, strnlen(kmsg_buf, KMSG_MAX_LEN));
	if (write_cnt < 0) {
		perror(KMSG_PREFIX"write kmsg failed");
		close(kmsg_fd);
		kmsg_fd = -1;
	}
}

static struct logger_ops logger_kmsg = {
	.name = "kmsg",
	.is_enabled = is_kmsg_enabled,
	.get_log_level = get_kmsg_log_level,
	.init = init_kmsg_logger,
	.deinit = deinit_kmsg,
	.output = write_to_kmsg,
};

DEFINE_LOGGER_DEVICE(logger_kmsg);
