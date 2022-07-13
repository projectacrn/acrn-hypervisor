/*
 * Copyright (C)2021-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _CMD_H_
#define _CMD_H_
#include <pthread.h>
#include <sys/queue.h>


#define SYNC_CMD "sync"
#define ACK_SYNC "ack_sync"
#define REQ_SYS_SHUTDOWN "req_sys_shutdown"
#define ACK_REQ_SYS_SHUTDOWN "ack_req_sys_shutdown"
#define POWEROFF_CMD "poweroff_cmd"
#define ACK_POWEROFF "ack_poweroff"
#define ACK_TIMEOUT "ack_timeout"
#define REQ_USER_VM_SHUTDOWN  "req_user_vm_shutdown"
#define USER_VM_SHUTDOWN  "user_vm_shutdown"
#define REQ_USER_VM_REBOOT  "req_user_vm_reboot"
#define USER_VM_REBOOT  "user_vm_reboot"
#define REQ_SYS_REBOOT "req_sys_reboot"
#define ACK_REQ_SYS_REBOOT "ack_req_sys_reboot"

#define ACK_REQ_USER_VM_SHUTDOWN  "ack_req_user_vm_shutdown"
#define ACK_USER_VM_SHUTDOWN "ack_user_vm_shutdown"
#define ACK_REQ_USER_VM_REBOOT  "ack_req_user_vm_reboot"
#define ACK_USER_VM_REBOOT "ack_user_vm_reboot"
#define FAIL_CONNECT "fail_connect"
#define USER_VM_DISCONNECT "user_vm_disconnect"
#define S5_REJECTED	"system shutdown request is rejected"

#define SYNC_LEN (sizeof(SYNC_CMD))

#define POWEROFF "poweroff"
#define REBOOT "reboot"

#define CMD_NAME_MAX 32U

enum command_id {
	SYNC_ID = 0x0,
	ACKED_SYNC_ID,
	REQ_SYS_SHUTDOWN_ID,
	ACKED_REQ_SYS_SHUTDOWN_ID,
	POWEROFF_CMD_ID,
	ACKED_POWEROFF_ID,
	ACK_TIMEOUT_ID,
	REQ_USER_VM_SHUTDOWN_ID,
	USER_VM_SHUTDOWN_ID,
	ACK_USER_VM_SHUTDOWN_ID,
	REQ_USER_VM_REBOOT_ID,
	USER_VM_REBOOT_ID,
	ACK_USER_VM_REBOOT_ID,
	REQ_SYS_REBOOT_ID,
	CMD_END,
};

typedef int (cmd_handler)(void *arg, int fd);
struct command_handlers {
	void *arg;
	cmd_handler *fn;

	LIST_ENTRY(command_handlers) list;
};

struct command {
	const char name[CMD_NAME_MAX]; /**< command name */
	enum command_id id; /**< command id */

	/* command handler list */
	LIST_HEAD(cmd_handlers_list, command_handlers) cmd_handlers_head;
	pthread_mutex_t cmd_handler_mtx; /**< mutex to protect command handler list */
};
/**
 * @brief register command handler, other module can use this interface to
 * register multiple handler for one command.
 *
 * @param fn the command handler which will be registered
 * @param arg the parameter which will be passed into hanlder
 * @param cmd_name the command name
 */
int register_command_handler(cmd_handler *fn, void *arg, const char *cmd_name);
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
 * @param fd the file descriptor of the device
 * @return the flag indicates the state of command handler execution
 */
int dispatch_command_handlers(void *arg, int fd);
#endif
