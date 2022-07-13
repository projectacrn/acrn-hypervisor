/*
 * Project Acrn
 * Acrn-dm: pm-vuart
 *
 * Copyright (C) 2019-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/* vuart can be used communication between Service VM and User VM, here it is used as power manager control. */

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
#include <errno.h>

#include "vmmapi.h"
#include "monitor.h"
#include "pty_vuart.h"
#include "log.h"

#define SHUTDOWN_CMD  "shutdown"
#define CMD_LEN 16
#define MAX_NODE_PATH  128
#define SERVICE_VM_SOCKET_PORT 0x2000

static const char * const node_name[] = {
	"pty",
	"tty",
};

enum node_type_t {
	PTY_NODE,
	TTY_NODE,
	MAX_NODE_CNT,
};

static bool allow_trigger_s5;
static uint8_t node_index = MAX_NODE_CNT;
static char node_path[MAX_NODE_PATH];
static int node_fd = -1;
static int socket_fd = -1;
static pthread_t pm_monitor_thread;
static pthread_mutex_t pm_vuart_lock = PTHREAD_MUTEX_INITIALIZER;

static int vm_stop_handler(void *arg);
static struct monitor_vm_ops vm_ops = {
	.stop = vm_stop_handler,
};

/* it read from vuart, and if end is '\0' or '\n' or len = buff-len it will return */
static bool read_bytes(int fd, uint8_t *buffer, int buf_len, int *count, bool *eof)
{
	bool ready = false;
	int rc = -1;

	if (buf_len <= (*count)) {
		*count = buf_len;
		ready = true;
		goto out;
	}

	do {
		rc = read(fd, buffer + (*count), buf_len - (*count));
		if (rc > 0) {
			*count += rc;
			if ((buffer[*count - 1] == '\0') || (buffer[*count - 1] == '\n') ||
					(*count == buf_len))
				ready = true;
		}
	} while (rc > 0 && !ready);

out:
	*eof = (rc == 0);
	return ready;
}

static int pm_setup_socket(void)
{
	struct sockaddr_in socket_addr;

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1) {
		pr_err("create a socket endpoint error\n");
		return -1;
	}

	memset(&socket_addr, 0, sizeof(struct sockaddr_in));
	socket_addr.sin_family = AF_INET;
	socket_addr.sin_port = htons(SERVICE_VM_SOCKET_PORT);
	socket_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(socket_fd, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) == -1) {
		pr_err("initiate a connection on a socket error\n");
		close(socket_fd);
		socket_fd = -1;
		return -1;
	}

	return 0;
}

static void *pm_monitor_loop(void *arg)
{
	int rc;
	bool eof;
	char buf_node[CMD_LEN+1], buf_socket[CMD_LEN+1];
	int max_fd, count_node = 0, count_socket = 0;
	fd_set read_fd;

	buf_node[CMD_LEN] = buf_socket[CMD_LEN] = '\0';
	max_fd = (socket_fd > node_fd) ? (socket_fd + 1) : (node_fd + 1);

	while (1) {
		FD_ZERO(&read_fd);
		FD_SET(socket_fd, &read_fd);
		FD_SET(node_fd, &read_fd);

		rc = select(max_fd, &read_fd, NULL, NULL, NULL);
		if (rc > 0) {
			if (FD_ISSET(node_fd, &read_fd)) {
				if (read_bytes(node_fd, (uint8_t *)buf_node, CMD_LEN,
						&count_node, &eof)) {
					pr_info("Received msg[%s] from User VM, count=%d\r\n",
						buf_node, count_node);
					rc = write(socket_fd, buf_node, count_node);

					if (rc != count_node) {
						pr_err("%s:%u: write error ret_val = %d\r\n",
							__func__, __LINE__, rc);
						break;
					}
					count_node = 0;
				}
			}
			if (FD_ISSET(socket_fd, &read_fd)) {
				if (read_bytes(socket_fd, (uint8_t *)buf_socket, CMD_LEN,
						&count_socket, &eof)) {
					pr_info("Received msg[%s] from life_mngr on Service VM, count=%d\r\n",
						buf_socket, count_socket);
					pthread_mutex_lock(&pm_vuart_lock);
					rc = write(node_fd, buf_socket, count_socket);
					pthread_mutex_unlock(&pm_vuart_lock);

					if (rc != count_socket) {
						pr_err("%s:%u: write error ret_val = %d\r\n",
							__func__, __LINE__, rc);
						break;
					}
					count_socket = 0;
				} else if (eof) {
					pr_err("socket connection to life-cycle manager closed\n");
					break;
				}
			}
		}
	}

	/* power off this VM if we get here */
	raise(SIGHUP);
	/* cleanup will be done in pm_by_vuart_deinit() */
	return NULL;
}

static int start_pm_monitor_thread(void)
{
	int ret;

	if (pm_setup_socket()) {
		pr_err("create socket to connect life-cycle manager failed\n");
		return -1;
	}

	if ((ret = pthread_create(&pm_monitor_thread, NULL, pm_monitor_loop, NULL))) {
		pr_err("%s: pthread_create error: %s\n", __func__, strerror(ret));
		close(socket_fd);
		socket_fd = -1;
		return -1;
	}

	pthread_setname_np(pm_monitor_thread, "pm_monitor");
	return 0;
}

/*
 * --pm_vuart configuration is in the following 2 forms:
 * A: pty-link, like: pty,/run/acrn/vuart-vm1, (also set it in -l com2,/run/acrn/vuart-vm1)
 * the Service VM and User VM will communicate by: (Service VM):pty-link-node <--> (Service VM):com2 <--> (User VM): /dev/ttyS1
 * B: tty-node, like: tty,/dev/ttyS1, (Service VM) and (User VM) communicate by: (Service VM):ttyS1 <--> HV <-->(User VM):ttySn
 */
int parse_pm_by_vuart(const char *opts)
{
	int i, error = -1;
	char *str, *cpy, *type;

	str = cpy = strdup(opts);
	if (!str) {
		pr_err("Function strdup return %d in %s line %d!\n", errno, __func__, __LINE__);
		return error;
	}
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

	pr_dbg("pm by vuart node-index = %d\n", node_index);
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

int pm_by_vuart_init(struct vmctx *ctx, bool trigger_s5)
{
	assert(node_index < MAX_NODE_CNT);

	allow_trigger_s5 = trigger_s5;
	pr_info("%s: allow_trigger_s5: %u, idx: %u, path: %s\r\n",
			__func__, trigger_s5, node_index, node_path);

	if (node_index == PTY_NODE)
		node_fd = pty_open_virtual_uart(node_path);
	else if (node_index == TTY_NODE)
		node_fd = open(node_path, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (node_fd >= 0) {
		if (node_index == TTY_NODE)
			set_tty_attr(node_fd, B115200);
		if (monitor_register_vm_ops(&vm_ops, ctx, "pm-vuart") < 0) {
			pr_err("%s: pm-vuart register to VM monitor failed\n", node_path);
			close(node_fd);
			node_fd = -1;
			return -1;
		}
	} else {
		pr_err("%s open failed, fd=%d\n", node_path, node_fd);
		return -1;
	}

	if (trigger_s5 && start_pm_monitor_thread()) {
		close(node_fd);
		node_fd = -1;
		return -1;
	}

	return 0;
}

void pm_by_vuart_deinit(struct vmctx *ctx)
{
	if (allow_trigger_s5) {
		pthread_cancel(pm_monitor_thread);
		pthread_join(pm_monitor_thread, NULL);
		close(socket_fd);
		socket_fd = -1;
	}
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

	pthread_mutex_lock(&pm_vuart_lock);
	ret = write(node_fd, SHUTDOWN_CMD, sizeof(SHUTDOWN_CMD));
	pthread_mutex_unlock(&pm_vuart_lock);
	if (ret != sizeof(SHUTDOWN_CMD)) {
		/* no need to resend shutdown here, will resend in pm_monitor thread */
		pr_err("send shutdown command to User VM failed\r\n");
	}

	return 0;
}
