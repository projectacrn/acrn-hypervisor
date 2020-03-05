/*
 * Project Acrn
 * Acrn-dm: pm-vuart
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/* vuart can be used communication between SOS and UOS, here it is used as power manager control. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>

#include "vmmapi.h"
#include "monitor.h"
#include "pty_vuart.h"
#include "log.h"

#define SHUTDOWN_CMD  "shutdown"
#define SHUTDOWN_CMD_ACK   "acked"
#define CMD_LEN 16
#define RESEND_CMD_CNT  3
#define MAX_NODE_PATH  128
#define SOS_SOCKET_PORT 0x2000

static const char * const node_name[] = {
	"pty",
	"tty",
};

enum node_type_t {
	PTY_NODE,
	TTY_NODE,
	MAX_NODE_CNT,
};

/* Enumerated shutdown state machine */
enum shutdown_state {
	SHUTDOWN_REQ_WAITING = 0,        /* Can receive shutdown cmd in this state */
	SHUTDOWN_ACK_WAITING,		 /* Wait acked message from UOS */
	SHUTDOWN_REQ_FROM_SOS,		 /* Trigger shutdown by SOS */
	SHUTDOWN_REQ_FROM_UOS,           /* Trigger shutdown by UOS */
};

static uint8_t node_index = MAX_NODE_CNT;
static char node_path[MAX_NODE_PATH];
static int node_fd = -1;
static enum shutdown_state pm_monitor_state = SHUTDOWN_REQ_WAITING;
static pthread_t pm_monitor_thread;

static int vm_stop_handler(void *arg);
static struct monitor_vm_ops vm_ops = {
	.stop = vm_stop_handler,
};
/* it read from vuart, and if end is '\0' or '\n' or len = buff-len it will return */
static int read_bytes(int fd, uint8_t *buffer, int buf_len)
{
	int rc = 0, count = 0;

	do {
		rc = read(fd, buffer + count, buf_len - count);
		if (rc > 0) {
			count += rc;
			if ((buffer[count - 1] == '\0') || (buffer[count - 1] == '\n') ||
					(count == buf_len)) {
				break;
			}
		}
	} while (rc > 0);

	return count;
}

/*
 * acrn-dm receive shutdown command from UOS,
 * it will call this api to send shutdown to life_mngr
 * running on SOS
 */
int send_shutdown_to_lifemngr(void)
{
	int rc;
	char buffer[CMD_LEN];
	int socket_fd;
	struct sockaddr_in socket_addr;

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd  == -1) {
		pr_err("socket() error.\n");
		return -1;
	}

	memset(&socket_addr, 0, sizeof(struct sockaddr_in));
	socket_addr.sin_family = AF_INET;
	socket_addr.sin_port = htons(SOS_SOCKET_PORT);
	socket_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(socket_fd, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) == -1) {
		pr_err("connect() error.\n");
		close(socket_fd);
		return -1;
	}

	/* send shutdown command to lifecycle management process */
	if (write(socket_fd, SHUTDOWN_CMD, sizeof(SHUTDOWN_CMD)) <= 0) {
		pr_err("send shutdown cmd to lifecycle management process failed!\n");
		close(socket_fd);
		return -1;
	}

	/* wait the acked message from lifecycle management */
	rc = read(socket_fd, (uint8_t *)buffer, sizeof(SHUTDOWN_CMD_ACK));
	if ((rc > 0) && strncmp(buffer, SHUTDOWN_CMD_ACK, strlen(SHUTDOWN_CMD_ACK)) == 0) {
		pr_info("received acked from lifecycle management process\n");
	} else {
		pr_err("received acked from lifecycle management failed!\n");
		close(socket_fd);
		return -1;
	}
	close(socket_fd);
	return 0;
}

/*
 * this pm_monitor can do:
 * --send shutdown request to UOS
 * --receive acked message from UOS
 * --receive shutdown request from UOS
 */
static void *pm_monitor_loop(void *arg)
{
	int rc;
	int retry = RESEND_CMD_CNT;
	char buf[CMD_LEN];

	while (1) {
		rc = read_bytes(node_fd, (uint8_t *)buf, CMD_LEN);
		switch (pm_monitor_state) {
		/* it can receive shutdown command from UOS for the following two states,
		 * it will change the state to SHUTDOWN_REQ_FROM_UOS if receive the shutdown
		 * command from UOS, this can prevent the follow-up shutdown command
		 * from UOS or SOS in this state.
		 * it will wait UOS poweroff itself and then send shutdown to
		 * life_mngr running on the SOS to shutdown system
		 */
		case SHUTDOWN_REQ_WAITING:
		case SHUTDOWN_REQ_FROM_UOS:
			if ((rc > 0) && (strncmp(SHUTDOWN_CMD, (const char *)buf, strlen(SHUTDOWN_CMD)) == 0)) {
				pm_monitor_state = SHUTDOWN_REQ_FROM_UOS;
				if (write(node_fd, SHUTDOWN_CMD_ACK, sizeof(SHUTDOWN_CMD_ACK))
						!= sizeof(SHUTDOWN_CMD_ACK)) {
					/* here no need to resend ack, it will resend shutdown cmd
					 * if uos can not receive acked message
					 */
					pr_err("send acked message to UOS failed!\n");
				}
			}
			break;

		/* Waiting acked message from UOS, this is triggered by SOS */
		case SHUTDOWN_ACK_WAITING:
			/* Once received the SHUTDOWN ACK from UOS, then wait UOS to set ACPI PM register to change
			 * VM to POWEROFF state
			 */
			if ((rc > 0) && (strncmp(SHUTDOWN_CMD_ACK, (const char *)buf, strlen(SHUTDOWN_CMD_ACK)) == 0)) {
				pr_info("received acked message from uos\n");
			} else {
				/* it will try to resend shutdown cmd to UOS if there is no acked message from UOS */
				if (retry > 0) {
					if (write(node_fd, SHUTDOWN_CMD, sizeof(SHUTDOWN_CMD))
							!= sizeof(SHUTDOWN_CMD)) {
						pr_err("Try resend shutdown cmd failed cnt = %d\n", retry);
					}
					retry--;
				} else {
					/* If there is no acked message from UOS, just ignore the shutdown request
					 * from SOS, and the SOS can re-trigger shutdown flow,
					 * so it need restore the pm_monitor_state
					 */
					pr_err("Can not receive acked message from uos, have try %d times\r\n",
							RESEND_CMD_CNT);
					pm_monitor_state = SHUTDOWN_REQ_WAITING;
					retry = RESEND_CMD_CNT;
				}
			}
			break;

		default:
			pr_err("Invalid pm_monitor_state(0x%x)\r\n", pm_monitor_state);
			break;
		}
		sleep(1);

	}

	return NULL;
}

static int start_pm_monitor_thread(void)
{
	int ret;

	ret = pthread_create(&pm_monitor_thread, NULL, pm_monitor_loop, NULL);
	if (ret) {
		pr_err("failed %s %d\n", __func__, __LINE__);
		return -1;
	}

	pthread_setname_np(pm_monitor_thread, "pm_monitor");
	return 0;
}

/*
 * --pm_vuart configuration is in the following 2 forms:
 * A: pty-link, like: pty,/run/acrn/vuart-vm1, (also set it in -l com2,/run/acrn/vuart-vm1)
 * the SOS and UOS will communicate by: SOS:pty-link-node <--> SOS:com2 <--> UOS: /dev/ttyS1
 * B: tty-node, like: tty,/dev/ttyS1, SOS and UOS communicate by: SOS:ttyS1 <--> HV <-->UOS:ttySn
 */
int parse_pm_by_vuart(const char *opts)
{
	int i, error = -1;
	char *str, *cpy, *type;

	str = cpy = strdup(opts);
	type = strsep(&str, ",");

	if (type != NULL) {
		for (i = 0; i < MAX_NODE_CNT; i++) {
			if (strcasecmp(type, node_name[i]) == 0) {
				node_index = i;
				error = 0;
				break;
			}
		}
	}

	printf("pm by vuart node-index = %d\n", node_index);
	strncpy(node_path, str, MAX_NODE_PATH - 1);

	free(cpy);
	return error;
}

static int set_tty_attr(int fd, int speed)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0) {
		pr_err("error from tcgetattr\n");
		return -1;
	}
	cfsetospeed(&tty, (speed_t)speed);
	cfsetispeed(&tty, (speed_t)speed);

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
		pr_err("error from tcsetattr\n");
		return -1;
	}

	return 0;
}

void pm_by_vuart_init(struct vmctx *ctx)
{
	assert(node_index < MAX_NODE_CNT);

	pr_info("%s idx: %d, path: %s\r\n", __func__, node_index, node_path);

	if (node_index == PTY_NODE) {
		node_fd = pty_open_virtual_uart(node_path);
	} else if (node_index == TTY_NODE) {
		node_fd = open(node_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
		set_tty_attr(node_fd, B115200);
	}

	if (node_fd > 0) {
		if (monitor_register_vm_ops(&vm_ops, ctx, "pm-vuart") < 0) {
			pr_err("%s: pm-vuart register to VM monitor failed\n", node_path);
			close(node_fd);
			node_fd = -1;
		}
	} else {
		pr_err("%s open failed, fd=%d\n", node_path, node_fd);
	}

	start_pm_monitor_thread();
}

void pm_by_vuart_deinit(struct vmctx *ctx)
{
	/* it indicates that acrn-dm has received shutdown command
	 * from UOS in this state, and it will send shutdown command
	 * to life_mngr running on SOS to shutdown system after UOS
	 * has poweroff itself.
	 */
	if (pm_monitor_state == SHUTDOWN_REQ_FROM_UOS) {
		/* send shutdown command to life_mngr running on SOS */
		if (send_shutdown_to_lifemngr() != 0) {
			pr_err("send shutdown to life-management failed\r\n");
		}
	}

	pthread_cancel(pm_monitor_thread);
	pthread_join(pm_monitor_thread, NULL);

	close(node_fd);
	node_fd = -1;
}

/* called when acrn-dm receive stop command */
static int vm_stop_handler(void *arg)
{
	int ret;

	pr_info("pm-vuart stop handler called: node-index=%d\n", node_index);
	assert(node_index < MAX_NODE_CNT);

	if (node_fd <= 0) {
		pr_err("no vuart node opened!\n");
		return -1;
	}

	/* will ignore this shutdown command if it is shutdowning */
	if (pm_monitor_state != SHUTDOWN_REQ_WAITING) {
		pr_err("can not shutdown in this state(0x%x),it is shutdowning\n", pm_monitor_state);
		return -1;
	}

	pm_monitor_state = SHUTDOWN_REQ_FROM_SOS;
	ret = write(node_fd, SHUTDOWN_CMD, sizeof(SHUTDOWN_CMD));
	if (ret != sizeof(SHUTDOWN_CMD)) {
		/* no need to resend shutdown here, will resend in pm_monitor thread */
		pr_err("send shutdown command to uos failed\r\n");
	}

	/* it will handle acked and resend shutdown cmd in pm_monitor thread */
	pm_monitor_state = SHUTDOWN_ACK_WAITING;

	return 0;
}
