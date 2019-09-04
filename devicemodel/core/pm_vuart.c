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

#include "vmmapi.h"
#include "monitor.h"
#include "pty_vuart.h"
#include "log.h"

#define SHUTDOWN_UOS_CMD  "shutdown"
#define SHUTDOWN_CMD_ACK   "acked"
#define CMD_LEN 16
#define WAIT_SND_CNT  3
#define WAIT_ACK_CNT  5
#define MAX_NODE_PATH  128

static const char * const node_name[] = {
	"pty",
	"tty",
};

enum node_type_t {
	PTY_NODE,
	TTY_NODE,
	MAX_NODE_CNT,
};

static uint8_t node_index = MAX_NODE_CNT;
static char node_path[MAX_NODE_PATH];
static int node_fd = -1;
static bool shutdown_uos_thread_started = false;
static pthread_t shutdown_uos_thread_pid;

static int vm_stop_handler(void *arg);
static struct monitor_vm_ops vm_ops = {
	.stop = vm_stop_handler,
};

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

	pr_info("pm by vuart node-index = %d\n", node_index);
	strncpy(node_path, str, MAX_NODE_PATH - 1);

	free(cpy);
	return error;
}

void pm_by_vuart_init(struct vmctx *ctx)
{
	assert(node_index < MAX_NODE_CNT);

	if (node_index == PTY_NODE) {
		node_fd = pty_open_virtual_uart(node_path);
	} else if (node_index == TTY_NODE) {
		node_fd = open(node_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
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
}

void pm_by_vuart_deinit(struct vmctx *ctx)
{
	close(node_fd);
	node_fd = -1;
}

/* it read from vuart, and if end is '\0' or '\n' or len = buff-len it will return */
static int read_bytes(int fd, uint8_t *buffer, int buf_len)
{
	int rc, offset = 0;

	do {
		rc = read(fd, buffer + offset, buf_len - offset);
		if (rc > 0) {
			offset += rc;

			if ((buffer[offset - 1] == '\0') || (buffer[offset - 1] == '\n') ||
				(offset == buf_len)) {
				break;
			}
		}
	} while (rc > 0);

	return offset;
}

/* thread to send shutdown cmd to uos and wait acked from it */
static void *shutdown_uos_thread(void *arg)
{
	int rc, wait_snd, wait_ack;
	char buffer[CMD_LEN];

	wait_snd = WAIT_SND_CNT;
	while (wait_snd > 0) {
		rc = write(node_fd, SHUTDOWN_UOS_CMD, strlen(SHUTDOWN_UOS_CMD));
		if (rc < 0) {
			pr_err("send shutdown cmd to uos failed!\n");
			break;
		}

		sleep(1); /* wait 1 second to communicate with UOS */

		wait_ack = WAIT_ACK_CNT;
		while (wait_ack > 0) {
			rc = read_bytes(node_fd, (uint8_t *)buffer, CMD_LEN - 1);
			if ((rc > 0) && strncmp(buffer, SHUTDOWN_CMD_ACK, strlen(SHUTDOWN_CMD_ACK)) == 0) {
				pr_info("received acked from UOS\n");
				break;
			}

			sleep(1);
			wait_ack--;
		}

		if (wait_ack > 0) {
			break;
		}

		wait_snd--;
	}

	if ((wait_snd == 0) && (wait_ack == 0)) {
		pr_err("not received acked from UOS\n");
	}

	shutdown_uos_thread_started = false;
	return NULL;
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

	if (shutdown_uos_thread_started) {
		pr_info("stop cmd send thread started!\n");
		return -1;
	}

	shutdown_uos_thread_started = true;
	ret = pthread_create(&shutdown_uos_thread_pid, NULL, shutdown_uos_thread, NULL);
	if (ret) {
		pr_err("failed %s %d\n", __func__, __LINE__);
		shutdown_uos_thread_pid = 0;
		shutdown_uos_thread_started = false;
		return -1;
	}

	pr_info("created shutdown uos thread.\n");
	pthread_setname_np(shutdown_uos_thread_pid, "shutdown_uos");

	return 0;
}
