/*
 * Project Acrn
 * Acrn-dm-pty
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <termios.h>
#include <stdbool.h>
#include <libgen.h>

#include "log.h"

/*
 * Check and create the directory.
 * To avoid symlink failure if the directory does not exist.
 */
static int check_dir(const char *file)
{
	char *tmp, *dir;

	tmp = strdup(file);
	if (!tmp) {
		pr_err("failed to dup file, error:%s\n", strerror(errno));
		return -1;
	}

	dir = dirname(tmp);
	if (access(dir, F_OK) && mkdir(dir, 0666)) {
		pr_err("failed to create dir:%s, erorr:%s\n", dir, strerror(errno));
		free(tmp);
		return -1;
	}
	free(tmp);
	return 0;
}

/*
 * Open PTY master device to used by caller and the PTY slave device for virtual
 * UART. The pair(master/slave) can work as a communication channel between
 * the caller and virtual UART.
 */
int pty_open_virtual_uart(const char *dev_name)
{
	int fd;
	char *slave_name;
	struct termios attr;

	fd = open("/dev/ptmx", O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0)
		goto open_err;
	if (grantpt(fd) < 0)
		goto pty_err;
	if (unlockpt(fd) < 0)
		goto pty_err;
	slave_name = ptsname(fd);
	if (!slave_name)
		goto pty_err;
	if ((unlink(dev_name) < 0) && errno != ENOENT)
		goto pty_err;
	/*
	 * The check_dir restriction is that only create one directory
	 * not support multi-level directroy.
	 */
	if (check_dir(dev_name) < 0)
		goto pty_err;
	if (symlink(slave_name, dev_name) < 0)
		goto pty_err;
	if (chmod(dev_name, 0660) < 0)
		goto attr_err;
	if (tcgetattr(fd, &attr) < 0)
		goto attr_err;
	cfmakeraw(&attr);
	attr.c_cflag |= CLOCAL;
	if (tcsetattr(fd, TCSANOW, &attr) < 0)
		goto attr_err;
	return fd;

attr_err:
	unlink(dev_name);
pty_err:
	close(fd);
open_err:
	return -1;
}
