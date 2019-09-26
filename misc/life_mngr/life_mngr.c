/*
 * Copyright (C)2019 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define SOS_REQ		"shutdown"
#define UOS_ACK		"acked"
#define BUFF_SIZE	16U
#define MSG_SIZE	8U
#define NODE_SIZE	3U

enum nodetype {
	NODE_UNKNOWN = 0,
	NODE_UOS_SERVER,
	NODE_SOS_CLIENT,
};

int set_serial_interface_attributes(int fd, int speed)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0) {
		printf("Error from tcgetattr: %s\n", strerror(errno));
		return errno;
	}

	cfsetospeed(&tty, (speed_t)speed);
	cfsetispeed(&tty, (speed_t)speed);

	/* set input-mode */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);

	/* set output-mode */
	tty.c_oflag &= ~OPOST;

	/* set control-mode */
	tty.c_cflag |= (CLOCAL | CREAD);
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	/* set local-mode */
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	/* block until one char read, set next char's timeout */
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 1;

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		printf("Error from tcsetattr: %s\n", strerror(errno));
		return errno;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	char *devname_uos = "";
	int fd_uos = 0;
	unsigned char recvbuf[BUFF_SIZE];
	enum nodetype node = NODE_UNKNOWN;
	int ret = 0;

	if (argc <= 2) {
		printf("Too few options. Example: [./life_mngr uos /dev/ttyS1].\n");
		return -EINVAL;
	}

	if (strncmp("uos", argv[1], NODE_SIZE) == 0) {
		node = NODE_UOS_SERVER;
	} else if (strncmp("sos", argv[1], NODE_SIZE) == 0) {
		node = NODE_SOS_CLIENT;
	} else {
		printf("Invalid param. Example: [./life_mngr uos /dev/ttyS1].\n");
		return -EINVAL;
	}

	if (node == NODE_UOS_SERVER) {
		devname_uos = argv[2];
		fd_uos = open(devname_uos, O_RDWR | O_NOCTTY | O_SYNC);
		if (fd_uos < 0) {
			printf("Error opening %s: %s\n", devname_uos, strerror(errno));
			return errno;
		}
		set_serial_interface_attributes(fd_uos, B115200);
	}

	/* UOS-server wait for shutdown from SOS */
	do {
		if (node == NODE_UOS_SERVER) {
			memset(recvbuf, 0, sizeof(recvbuf));
			ret = read(fd_uos, recvbuf, sizeof(recvbuf));

			if (strncmp(SOS_REQ, (const char *)recvbuf, MSG_SIZE) == 0) {
				ret = write(fd_uos, UOS_ACK, sizeof(UOS_ACK));
				if (ret != sizeof(UOS_ACK)) {
					printf("UOS acked fail\n");
				}
				printf("SOS start shutdown\n");
				ret = system("poweroff");
				break;
			}
		}
	} while (1);

	return ret;
}
