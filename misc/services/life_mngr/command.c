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
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include "command.h"
#include "log.h"

#define GEN_CMD_OBJ(cmd_name, cmd_id) \
	{.name = cmd_name, .id = cmd_id, .cmd_handler_mtx = PTHREAD_MUTEX_INITIALIZER}
#define CMD_OBJS \
	GEN_CMD_OBJ(SYNC_CMD, SYNC_ID), \
	GEN_CMD_OBJ(ACK_SYNC, ACKED_SYNC_ID), \
	GEN_CMD_OBJ(REQ_SYS_SHUTDOWN, REQ_SYS_SHUTDOWN_ID), \
	GEN_CMD_OBJ(ACK_REQ_SYS_SHUTDOWN, ACKED_REQ_SYS_SHUTDOWN_ID), \
	GEN_CMD_OBJ(POWEROFF_CMD, POWEROFF_CMD_ID), \
	GEN_CMD_OBJ(ACK_POWEROFF, ACKED_POWEROFF_ID), \
	GEN_CMD_OBJ(ACK_TIMEOUT, ACK_TIMEOUT_ID), \
	GEN_CMD_OBJ(REQ_USER_VM_SHUTDOWN, REQ_USER_VM_SHUTDOWN_ID), \
	GEN_CMD_OBJ(USER_VM_SHUTDOWN, USER_VM_SHUTDOWN_ID),\
	GEN_CMD_OBJ(ACK_USER_VM_SHUTDOWN, ACK_USER_VM_SHUTDOWN_ID),\
	GEN_CMD_OBJ(REQ_USER_VM_REBOOT, REQ_USER_VM_REBOOT_ID), \
	GEN_CMD_OBJ(USER_VM_REBOOT, USER_VM_REBOOT_ID),\
	GEN_CMD_OBJ(ACK_USER_VM_REBOOT, ACK_USER_VM_REBOOT_ID),\
	GEN_CMD_OBJ(REQ_SYS_REBOOT, REQ_SYS_REBOOT_ID), \

struct command dm_command_list[CMD_END] = {CMD_OBJS};

int dispatch_command_handlers(void *arg, int fd)
{
	struct command *cmd = (struct command *)arg;
	struct command_handlers *handler;
	unsigned int count = 0U;
	int ret = 0;

	if (cmd == NULL) {
		LOG_PRINTF("Invalid command, fd=%d\n", fd);
		return -EINVAL;
	}
	LOG_PRINTF("Handle command (%s) in command monitor\n", cmd->name);
	LIST_FOREACH(handler, &cmd->cmd_handlers_head, list) {
		if (handler->fn) {
			ret = handler->fn(handler->arg, fd);
			count++;
		}
	}
	LOG_PRINTF("Command handler ret=%d\n", ret);
	if (!count)
		LOG_PRINTF("No handler for command:%s\r\n", cmd->name);
	return 0;
}
struct command *find_command(const char *name)
{
	for (int i = 0; (i < CMD_END) && (name != NULL); i++) {
		if (strcmp(dm_command_list[i].name, name) == 0)
			return &dm_command_list[i];
	}
	return NULL;
}

int register_command_handler(cmd_handler *fn, void *arg, const char *cmd_name)
{
	struct command *cmd;
	struct command_handlers *handler;

	if ((!fn) || (!arg) || (!cmd_name)) {
		LOG_PRINTF("%s:Failed to register command_handler\n", __func__);
		return -EINVAL;
	}
	cmd = find_command(cmd_name);
	if (cmd == NULL) {
		LOG_PRINTF("%s:invalid command name (%s)\r\n", __func__, cmd_name);
		return -EINVAL;
	}

	handler = calloc(1, sizeof(*handler));
	if (!handler) {
		LOG_WRITE("Failed to allocate command handler\r\n");
		return -ENOMEM;
	}

	handler->fn = fn;
	handler->arg = arg;

	pthread_mutex_lock(&cmd->cmd_handler_mtx);
	LIST_INSERT_HEAD(&cmd->cmd_handlers_head, handler, list);
	pthread_mutex_unlock(&cmd->cmd_handler_mtx);

	return 0;
}
