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
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include "command.h"
#include "vmmapi.h"
#include "log.h"

#define GEN_CMD_OBJ(cmd_name) \
	{.name = cmd_name,}
#define CMD_OBJS \
	GEN_CMD_OBJ(DESTROY), \
	GEN_CMD_OBJ(BLKRESCAN), \

struct command dm_command_list[CMDS_NUM] = {CMD_OBJS};

int dispatch_command_handlers(void *arg)
{
	struct command *cmd = (struct command *)arg;
	int ret = 0;

	pr_info("Handle command %s in command monitor.\n", cmd->name);
	if (cmd->cmd_handler.fn) {
		ret = cmd->cmd_handler.fn(cmd->cmd_handler.arg, &cmd->para);
		pr_info("Command handler ret=%d.\n", ret);
	} else {
		pr_info("No handler for command: %s.\r\n", cmd->name);
	}

	return 0;
}
struct command *find_command(const char *name)
{
	for (int i = 0; (i < CMDS_NUM) && (name != NULL); i++) {
		if (strcmp(dm_command_list[i].name, name) == 0)
			return &dm_command_list[i];
	}
	return NULL;
}

int register_command_handler(cmd_handler *fn, struct handler_args *arg, const char *cmd_name)
{
	struct command *cmd;
	struct handler_args *handler_arg;

	if ((!fn) || (!arg) || (!cmd_name)) {
		pr_err("%s : Failed to register command_handler.\n", __func__);
		return -EINVAL;
	}

	cmd = find_command(cmd_name);
	if (cmd == NULL) {
		pr_err("%s : invalid command name %s.\r\n", __func__, cmd_name);
		return -EINVAL;
	}

	if (cmd->cmd_handler.fn != NULL) {
		pr_err("Failed to register command handler since the handler have already been registered.\n");
		return -EINVAL;
	}
	cmd->cmd_handler.fn = fn;

	handler_arg = calloc(1, sizeof(*handler_arg));
	if (!handler_arg) {
		pr_err("Failed to allocate command handler argument.\r\n");
		return -ENOMEM;
	}
	cmd->cmd_handler.arg = handler_arg;
	cmd->cmd_handler.arg->channel_arg = arg->channel_arg;
	cmd->cmd_handler.arg->ctx_arg = arg->ctx_arg;

	return 0;
}
