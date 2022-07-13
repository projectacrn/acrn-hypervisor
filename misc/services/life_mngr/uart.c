/*
 * Copyright (C)2021-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/queue.h>
#include <pthread.h>
#include <limits.h>
#include "uart.h"
#include "uart_channel.h"
#include "log.h"
#include "config.h"

/* it read from uart, and if end is '\0' or '\n' or len = buff-len it will return */
static ssize_t try_receive_message_by_uart(int fd, void *buffer, size_t buf_len)
{
	ssize_t rc = 0U, count = 0U;
	char *tmp;
	unsigned int retry_times = RETRY_RECV_TIMES;

	do {
		/* NOTE: Now we can't handle multi command message at one time. */
		rc = read(fd, buffer + count, buf_len - count);
		if (rc > 0) {
			count += rc;
			tmp = (char *)buffer;
			if ((tmp[count - 1] == '\0') || (tmp[count - 1] == '\n')
						|| (count == buf_len)) {
				if (tmp[count - 1] == '\n')
					tmp[count - 1] = '\0';
				break;
			}
		} else {
			if (errno == EAGAIN) {
				usleep(WAIT_RECV);
				retry_times--;
			} else {
				break;
			}
		}
	} while (retry_times != 0U);

	return count;
}
/**
 * @brief Receive message and retry RETRY_RECV_TIMES time if
 * message is missed in some cases.
 */
ssize_t receive_message_by_uart(struct uart_dev *dev, void *buf, size_t len)
{
	if ((dev == NULL) || (buf == NULL) || (len == 0))
		return -EINVAL;

	return try_receive_message_by_uart(dev->tty_fd, buf, len);
}
ssize_t send_message_by_uart(struct uart_dev *dev, const void *buf, size_t len)
{
	ssize_t ret = 0;

	if ((dev == NULL) || (buf == NULL) || (len == 0))
		return -EINVAL;
	ret = write(dev->tty_fd, buf, len + 1);

	return ret;
}
static int set_tty_attr(int fd, int baudrate)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0) {
		LOG_PRINTF("Error from tcgetattr: %s\n", strerror(errno));
		return errno;
	}

	cfsetospeed(&tty, (speed_t)baudrate);
	cfsetispeed(&tty, (speed_t)baudrate);

	/* set input-mode */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK |
			ISTRIP | INLCR | IGNCR | ICRNL | IXON);

	/* set output-mode */
	tty.c_oflag &= ~OPOST;

	/* set control-mode */
	tty.c_cflag |= (CLOCAL | CREAD | CS8);
	tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);

	/* set local-mode */
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	/* block until one char read, set next char's timeout */
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 1;

	tcflush(fd, TCIOFLUSH);

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		LOG_PRINTF("Error from tcsetattr: %s\n", strerror(errno));
		return errno;
	}
	return 0;
}
static int tty_listen_setup(const char *tty_path)
{
	int fd = -1;
	int ret;

	fd = open(tty_path, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
	if (fd < 0) {
		fprintf(stderr, "Can't open file %s\n", tty_path);
		return errno;
	}
	ret = set_tty_attr(fd, B115200);
	if (ret != 0) {
		close(fd);
		return ret;
	}
	LOG_PRINTF("Open tty device:%s, fd=%d\n", tty_path, fd);
	return fd;
}

struct uart_dev *init_uart_dev(char *path)
{
	struct uart_dev *dev;

	if (path == NULL)
		return NULL;
	LOG_PRINTF("UART device name:%s\n", path);
	dev = calloc(1, sizeof(*dev));
	if (!dev) {
		LOG_PRINTF("Failed to alloc mem for uart device %s\n", path);
		return NULL;
	}
	if (strlen(path) < TTY_PATH_MAX)
		memcpy(dev->tty_path, path, strlen(path));

	dev->tty_fd = tty_listen_setup(dev->tty_path);
	if (dev->tty_fd < 0) {
		LOG_PRINTF("Failed to setup uart device %s\n", path);
		free(dev);
		return NULL;
	}
	return dev;
}
void deinit_uart_dev(struct uart_dev *dev)
{
	if (dev != NULL) {
		LOG_PRINTF("Close device: %s\n", dev->tty_path);
		close(dev->tty_fd);
		dev->tty_fd = -1;
		free(dev);
	}
}

