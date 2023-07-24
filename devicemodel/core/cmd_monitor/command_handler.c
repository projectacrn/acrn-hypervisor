/*
 * Copyright (C) 2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <libgen.h>
#include <cjson/cJSON.h>
#include "command.h"
#include "socket.h"
#include "command_handler.h"
#include "dm.h"
#include "pm.h"
#include "vmmapi.h"
#include "log.h"
#include "monitor.h"

#define SUCCEEDED 0
#define FAILED -1

static char *generate_ack_message(int ret_val)
{
	char *ack_msg;
	cJSON *val;
	cJSON *ret_obj = cJSON_CreateObject();

	if (ret_obj == NULL)
		return NULL;
	val = cJSON_CreateNumber(ret_val);
	if (val == NULL)
		return NULL;
	cJSON_AddItemToObject(ret_obj, "ack", val);
	ack_msg = cJSON_Print(ret_obj);
	if (ack_msg == NULL)
		fprintf(stderr, "Failed to generate ACK message.\n");
	cJSON_Delete(ret_obj);
	return ack_msg;
}
static int send_socket_ack(struct socket_dev *sock, int fd, bool normal)
{
	int ret = 0, val;
	char *ack_message;
	struct socket_client *client = NULL;

	client = find_socket_client(sock, fd);
	if (client == NULL)
		return -1;
	val = normal ? SUCCEEDED : FAILED;
	ack_message = generate_ack_message(val);

	if (ack_message != NULL) {
		memset(client->buf, 0, CLIENT_BUF_LEN);
		memcpy(client->buf, ack_message, strlen(ack_message));
		client->len = strlen(ack_message);
		ret = write_socket_char(client);
		free(ack_message);
	} else {
		pr_err("Failed to generate ACK message.\n");
		ret = -1;
	}
	return ret;
}

static struct socket_client *vm_event_client = NULL;
static pthread_mutex_t vm_event_client_mutex = PTHREAD_MUTEX_INITIALIZER;

static void vm_event_free_cb(struct socket_client *self)
{
	vm_event_client = NULL;
}

static int set_vm_event_client(struct socket_client *client)
{
	if (vm_event_client != NULL) {
		pr_err("vm event client already registerred.\n");
		return -1;
	} else {
		vm_event_client = client;
		client->per_client_mutex = &vm_event_client_mutex;
		client->free_client_cb = vm_event_free_cb;
		return 0;
	}
}

int vm_monitor_send_vm_event(const char *msg)
{
	int ret = -1;
	struct socket_client *client;
	pthread_mutex_t *per_client_mutex = &vm_event_client_mutex;

	pthread_mutex_lock(per_client_mutex);
	client = vm_event_client;
	if (msg == NULL || client == NULL) {
		pthread_mutex_unlock(per_client_mutex);
		return -1;
	}
	memset(client->buf, 0, CLIENT_BUF_LEN);
	memcpy(client->buf, msg, strlen(msg));
	client->len = strlen(msg);
	ret = write_socket_char(client);
	pthread_mutex_unlock(per_client_mutex);
	return ret;
}

/* When a client issues the REGISTER_VM_EVENT_CLIENT command,
 * this handler will register that client as this VM's only vm_event receiver,
 * and keeps the socket connection. Then vm events will be sent to
 * the client through this connection.
 */
int user_vm_register_vm_event_client_handler(void *arg, void *command_para)
{
	int ret;
	struct command_parameters *cmd_para = (struct command_parameters *)command_para;
	struct handler_args *hdl_arg = (struct handler_args *)arg;
	struct socket_dev *sock = (struct socket_dev *)hdl_arg->channel_arg;
	struct socket_client *client = NULL;
	bool cmd_completed = false;

	client = find_socket_client(sock, cmd_para->fd);
	if (client == NULL)
		return -1;

	if (set_vm_event_client(client) == 0) {
		cmd_completed = true;
	}

	pr_dbg("%s: client with fd %d registerred\n", __func__, client->fd);

	ret = send_socket_ack(sock, cmd_para->fd, cmd_completed);
	if (ret < 0) {
		pr_err("%s: Failed to send ACK message by socket.\n", __func__);
	}
	return ret;
}

int user_vm_destroy_handler(void *arg, void *command_para)
{
	int ret;
	struct command_parameters *cmd_para = (struct command_parameters *)command_para;
	struct handler_args *hdl_arg = (struct handler_args *)arg;
	struct socket_dev *sock = (struct socket_dev *)hdl_arg->channel_arg;
	struct socket_client *client = NULL;
	bool cmd_completed = false;

	client = find_socket_client(sock, cmd_para->fd);
	if (client == NULL)
		return -1;

	if (!is_rtvm) {
		pr_info("%s: setting VM state to %s.\n", __func__, vm_state_to_str(VM_SUSPEND_POWEROFF));
		vm_set_suspend_mode(VM_SUSPEND_POWEROFF);
		cmd_completed = true;
	} else {
		pr_err("Failed to destroy post-launched RTVM.\n");
		ret = -1;
	}

	ret = send_socket_ack(sock, cmd_para->fd, cmd_completed);
	if (ret < 0) {
		pr_err("Failed to send ACK message by socket.\n");
	}
	return ret;
}

int user_vm_blkrescan_handler(void *arg, void *command_para)
{
	int ret = 0;
	struct command_parameters *cmd_para = (struct command_parameters *)command_para;
	struct handler_args *hdl_arg = (struct handler_args *)arg;
	struct socket_dev *sock = (struct socket_dev *)hdl_arg->channel_arg;
	struct socket_client *client = NULL;
	bool cmd_completed = false;

	client = find_socket_client(sock, cmd_para->fd);
	if (client == NULL)
		return -1;

	ret = vm_monitor_blkrescan(hdl_arg->ctx_arg, cmd_para->option);
	if (ret >= 0) {
		cmd_completed = true;
	} else {
		pr_err("Failed to rescan virtio-blk device.\n");
	}

	ret = send_socket_ack(sock, cmd_para->fd, cmd_completed);
	if (ret < 0) {
		pr_err("Failed to send ACK by socket.\n");
	}
	return ret;
}
