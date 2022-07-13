/*
 * Copyright (C) 2022 Intel Corporation.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <cjson/cJSON.h>
#include "socket.h"
#include "command.h"
#include "vmmapi.h"
#include "cmd_monitor.h"
#include "log.h"
#include "dm.h"
#include "command_handler.h"

struct socket_dev *sock_server; /* socket server instance */
static char socket_path[UNIX_SOCKET_PATH_MAX];

static struct command *parse_command(char *cmd_msg, int fd)
{
	struct command *cmd = NULL;
	const cJSON *execute;
	const cJSON *arguments;
	cJSON *cmd_json;

	cmd_json = cJSON_Parse(cmd_msg);
	if (cmd_json == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL) {
			fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return NULL;
	}
	execute = cJSON_GetObjectItemCaseSensitive(cmd_json, "command");
	if (cJSON_IsString(execute) && (execute->valuestring != NULL)) {
		pr_info("Command name: \"%s\"\n", execute->valuestring);

		cmd = find_command(execute->valuestring);
		if (cmd != NULL) {
			cmd->para.fd = fd;
			arguments = cJSON_GetObjectItemCaseSensitive(cmd_json, "arguments");
			if (cJSON_IsString(arguments) && (arguments->valuestring != NULL)) {
				pr_info("Command arguments: \"%s\"\n", arguments->valuestring);
				strncpy(cmd->para.option, arguments->valuestring, CMD_ARG_MAX - 1U);
			}
		} else {
			pr_err("Command [%s] is not supported.\n", execute->valuestring);
		}
	}
	cJSON_Delete(cmd_json);
	return cmd;
}
static void monitor_cmd_dispatch(char *cmd_msg, int fd)
{
	struct command *cmd;

	cmd = parse_command(cmd_msg, fd);
	if (cmd != NULL) {
		dispatch_command_handlers(cmd);
	}
	return;
}

int init_socket_server(void)
{
	int ret = 0;

	if (strnlen(socket_path, UNIX_SOCKET_PATH_MAX) == 0) {
		pr_err("Failed to initialize command monitor due to invalid socket path.\n");
	}

	sock_server = init_socket(socket_path);
	if (sock_server == NULL)
		return -1;
	ret = open_socket(sock_server, monitor_cmd_dispatch);
	if (ret < 0)
		return ret;
	return ret;
}

static void register_socket_message_handlers(struct vmctx *ctx)
{
	struct handler_args arg;
	arg.channel_arg = sock_server;
	arg.ctx_arg = ctx;
	register_command_handler(user_vm_destroy_handler, &arg, DESTROY);
	register_command_handler(user_vm_blkrescan_handler, &arg, BLKRESCAN);
}

int init_cmd_monitor(struct vmctx *ctx)
{
	int ret;
	ret = init_socket_server();
	register_socket_message_handlers(ctx);
	return ret;
}
void deinit_cmd_monitor(void)
{
	if (sock_server != NULL) {
		close_socket(sock_server);
		deinit_socket(sock_server);
	}
}
int acrn_parse_cmd_monitor(char *arg)
{
	int err = -1;
	size_t len = strnlen(arg, UNIX_SOCKET_PATH_MAX);

	if (len < UNIX_SOCKET_PATH_MAX) {
		strncpy(socket_path, arg, len + 1);
		pr_notice("Command monitor: using soket path %s\n", socket_path);
		err = 0;
	}
	return err;
}
