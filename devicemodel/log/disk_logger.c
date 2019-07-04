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
#include <dirent.h>
#include <time.h>

#include "dm.h"
#include "log.h"

#define DISK_PREFIX  "disk_log: "

#define LOG_PATH_NODE "/var/log/acrn-dm/"
#define LOG_NAME_PREFIX  "%s_log_"
#define LOG_NAME_FMT  "%s%s_log_%d" /* %s-->vm1/vm2..., %d-->1/2/3/4... */
#define LOG_DELIMITER "\n\n----------------new vm instance------------------\n\n\0"

#define FILE_NAME_LENGTH  96

#define LOG_SIZE_LIMIT    0x200000 /* one log file size limit */
#define LOG_FILES_COUNT   8

static int disk_fd = -1;
static uint32_t cur_log_size;
static uint16_t cur_file_index;

static uint8_t disk_log_level = LOG_DEBUG;
static bool disk_log_enabled = false;

#define DISK_LOG_MAX_LEN    (MAX_ONE_LOG_SIZE + 32)
#define INDEX_AFTER(a, b) ((short int)b - (short int)a < 0)

static bool is_disk_log_enabled(void)
{
	return disk_log_enabled;
}

static uint8_t get_disk_log_level(void)
{
	return disk_log_level;
}

static int probe_disk_log_file(void)
{
	char file_name[FILE_NAME_LENGTH];
	struct dirent *pdir;
	struct stat st;
	int length;
	uint16_t index = 0, tmp;
	bool is_first_file = true;
	DIR *dir;

	if (stat(LOG_PATH_NODE, &st)) {
		if (mkdir(LOG_PATH_NODE, 0644)) {
			printf(DISK_PREFIX"create path: %s failed! Error: %s\n",
				LOG_PATH_NODE, strerror(errno));
			return -1;
		}
	}

	dir = opendir(LOG_PATH_NODE);
	if (!dir) {
		printf(DISK_PREFIX" open %s failed! Error: %s\n",
			LOG_PATH_NODE, strerror(errno));
		return -1;
	}

	snprintf(file_name, FILE_NAME_LENGTH - 1, LOG_NAME_PREFIX, vmname);
	length = strlen(file_name);

	while ((pdir = readdir(dir)) != NULL) {
		if (!(pdir->d_type & DT_REG))
			continue;

		if (strncmp(pdir->d_name, file_name, length) != 0)
			continue;

		tmp = (uint16_t)atoi(pdir->d_name + length);
		if (is_first_file) {
			is_first_file = false;
			index = tmp;
		} else if (INDEX_AFTER(tmp, index)) {
			index = tmp;
		}
	}

	snprintf(file_name, FILE_NAME_LENGTH - 1, LOG_NAME_FMT, LOG_PATH_NODE, vmname, index);
	disk_fd = open(file_name, O_RDWR | O_CREAT | O_APPEND, 0644);
	if (disk_fd < 0) {
		printf(DISK_PREFIX" open %s failed! Error: %s\n", file_name, strerror(errno));
		return -1;
	}

	if (write(disk_fd, LOG_DELIMITER, strlen(LOG_DELIMITER)) < 0) {
		printf(DISK_PREFIX" write %s failed! Error: %s\n", file_name, strerror(errno));
		return -1;
	}

	fstat(disk_fd, &st);
	cur_log_size = st.st_size;
	cur_file_index = index;

	return 0;
}

static int init_disk_logger(bool enable, uint8_t log_level)
{
	disk_log_enabled = enable;
	disk_log_level = log_level;

	return 1;
}

static void deinit_disk_logger(void)
{
	if (disk_fd > 0) {
		disk_log_enabled = false;

		fsync(disk_fd);
		close(disk_fd);
		disk_fd = -1;
	}
}

static void write_to_disk(const char *fmt, va_list args)
{
	char buffer[DISK_LOG_MAX_LEN];
	char *file_name = buffer;
	char *buf;
	int len;
	int write_cnt;
	struct timespec times = {0, 0};

	if ((disk_fd < 0) && disk_log_enabled) {
		/**
		 * usually this probe just be called once in DM whole life; but we need use vmname in
		 * probe_disk_log_file, it can't be called in init_disk_logger for vmname not inited then,
		 * so call it here.
		 */
		if (probe_disk_log_file() < 0) {
			disk_log_enabled = false;
			return;
		}
	}

	len = vasprintf(&buf, fmt, args);
	if (len < 0)
		return;

	clock_gettime(CLOCK_MONOTONIC, &times);
	len = snprintf(buffer, DISK_LOG_MAX_LEN, "[%5lu.%06lu] ", times.tv_sec, times.tv_nsec / 1000);
	if (len < 0 || len >= DISK_LOG_MAX_LEN) {
		free(buf);
		return;
	}
	len = strnlen(buffer, DISK_LOG_MAX_LEN);

	strncpy(buffer + len, buf, DISK_LOG_MAX_LEN - len);
	buffer[DISK_LOG_MAX_LEN - 1] = '\0';
	free(buf);

	write_cnt = write(disk_fd, buffer, strnlen(buffer, DISK_LOG_MAX_LEN));
	if (write_cnt < 0) {
		perror(DISK_PREFIX"write disk failed");
		close(disk_fd);
		disk_fd = -1;
		return;
	}

	cur_log_size += write_cnt;
	if (cur_log_size > LOG_SIZE_LIMIT) {

		cur_file_index++;

		/* remove the first old log file, to add a new one */
		snprintf(file_name, FILE_NAME_LENGTH - 1, LOG_NAME_FMT,
			LOG_PATH_NODE, vmname, (uint16_t)(cur_file_index - LOG_FILES_COUNT));
		remove(file_name);

		snprintf(file_name, FILE_NAME_LENGTH - 1, LOG_NAME_FMT,
			LOG_PATH_NODE, vmname, cur_file_index);

		close(disk_fd);
		disk_fd = open(file_name, O_RDWR | O_CREAT, 0644);
		if (disk_fd < 0) {
			printf(DISK_PREFIX" open %s failed! Error: %s\n", file_name, strerror(errno));
			return;
		}
		cur_log_size = 0;
	}
}

static struct logger_ops logger_disk = {
	.name = "disk",
	.is_enabled = is_disk_log_enabled,
	.get_log_level = get_disk_log_level,
	.init = init_disk_logger,
	.deinit = deinit_disk_logger,
	.output = write_to_disk,
};

DEFINE_LOGGER_DEVICE(logger_disk);
