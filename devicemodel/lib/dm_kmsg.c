/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include "dm_kmsg.h"

int  fd_kmsg;

static int open_kmsg(void);
static int close_kmsg(void);

static int open_kmsg(void)
{
	int fd;

	/* open /dev/kmsg */
	fd = open(KERN_NODE, O_RDWR | O_APPEND | O_NONBLOCK);
	if (fd < 0) {
		perror(KMSG_FMT"open_kmsg");
		return fd;
	}

	fd_kmsg = fd;
	return fd;
}

static int close_kmsg(void)
{
	int err;

	/* close /dev/kmsg */
	err = close(fd_kmsg);
	if (err == -1) {
		perror(KMSG_FMT"close_kmsg");
		return err;
	}

	return 0;
}

void write_kmsg(const char *fmt, ...)
{
	int write_cnt, len;
	va_list args;
	char *dm_buf;

	va_start(args, fmt);
	len = vasprintf(&dm_buf, fmt, args);
	va_end(args);
	if (len == -1) {
		return;
	}

	open_kmsg();

	/* write the fmt to the /dev/kmsg */
	write_cnt = write(fd_kmsg, dm_buf, len);
	if (write_cnt < 0) {
		perror(KMSG_FMT"write_kmsg");
	}

	close_kmsg();
	free(dm_buf);
	return;
}
