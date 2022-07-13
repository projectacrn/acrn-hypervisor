/*
 * Copyright (C) 2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _COMMAND_H_
#define _COMMAND_H_
#include <pthread.h>
#include <sys/queue.h>


#define DESTROY "destroy"
#define BLKRESCAN "blkrescan"

#define CMDS_NUM 2U
#define CMD_NAME_MAX 32U
#define CMD_ARG_MAX 320U

typedef int (cmd_handler)(void *arg, void *command_para);
struct handler_args {
	void *channel_arg;
	void *ctx_arg;
};
struct command_handler {
	struct handler_args *arg;
	cmd_handler *fn;
};
struct command_parameters {
	int fd;
	char option[CMD_ARG_MAX];
};
struct command {
	const char name[CMD_NAME_MAX]; /**< command name */
	struct command_parameters para;

	/* command handler */
	struct command_handler cmd_handler;
};
/**
 * @brief register command handler, other module can use this interface to
 * register multiple handler for one command.
 *
 * @param fn the command handler which will be registered
 * @param arg the parameter which will be passed into hanlder
 * @param cmd_name the command name
 */
int register_command_handler(cmd_handler *fn, struct handler_args *arg, const char *cmd_name);
/**
 * @brief find a command instance by name
 *
 * @param name the command name
 * @return command instance
 */
struct command *find_command(const char *name);
/**
 * @brief dispatch the command and invoke registered handler.
 *
 * @param arg command instance
 * @return the flag indicates the state of command handler execution
 */
int dispatch_command_handlers(void *arg);
#endif
