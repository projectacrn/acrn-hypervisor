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
#include <sys/stat.h>
#include <sys/file.h>
#include "uart_channel.h"
#include "command.h"
#include "socket.h"
#include "command_handler.h"
#include "log.h"
#include "config.h"

#define NODE_SIZE 5
#define S5_SOCKET_DIR "/var/lib/life_mngr"
#define S5_SOCKET_FMT "%s/monitor.sock"
#define SERVICE_VM_NAME "service_vm"

struct uart_channel *channel; /* uart server instance */
struct socket_dev *sock_server; /* socket server instance */

FILE *log_fd;

static void monitor_cmd_dispatch(const char *cmd_name, int fd)
{
	struct command *cmd;

	cmd = find_command(cmd_name);
	if (cmd != NULL)
		dispatch_command_handlers(cmd, fd);
	else
		LOG_PRINTF("Command [%s] is not supported, fd=%d\n", cmd_name, fd);
}
/**
 * @brief open uart channel according to device name
 *
 * @param uart_dev_name one or more device names
 * @return int all uart channel devices are open or not
 */
static int create_service_vm_uart_channel_dev(char *uart_dev_name)
{
	int ret = 0;
	struct channel_dev *c_dev;
	char *dev_name;
	char *saveptr;

	saveptr = uart_dev_name;
	do {
		dev_name = strtok_r(saveptr, ",", &saveptr);
		c_dev = create_uart_channel_dev(channel, dev_name, monitor_cmd_dispatch);
		if (c_dev == NULL) {
			LOG_PRINTF("Failed to create uart channel device for %s\n", dev_name);
			ret = -1;
			break;
		}
		pthread_create(&c_dev->listen_thread, NULL, listen_uart_channel_dev, c_dev);
		pthread_create(&c_dev->pool_thread, NULL, poll_and_dispatch_uart_channel_events, c_dev);
	} while (strlen(saveptr) > 0U);

	return ret;
}
/* TODO: will refine the name of init_socket_server_and_shutdown_commands */
int init_socket_server_and_shutdown_commands(bool service_vm)
{
	int ret = 0;
	char path[128] = S5_SOCKET_DIR;

	ret = check_dir(path, CHK_CREAT);
	if (ret < 0) {
		LOG_PRINTF("%s %d\r\n", __func__, __LINE__);
		return ret;
	}
	snprintf(path, sizeof(path), S5_SOCKET_FMT, S5_SOCKET_DIR);

	sock_server = init_socket(path);
	if (sock_server == NULL)
		return -1;
	ret = open_socket(sock_server, monitor_cmd_dispatch);
	if (ret < 0)
		return ret;
	if (service_vm) {
		register_command_handler(socket_req_shutdown_service_vm_handler,
						sock_server, REQ_SYS_SHUTDOWN);
		register_command_handler(socket_req_user_vm_shutdown_handler,
						sock_server, USER_VM_SHUTDOWN);
		register_command_handler(socket_req_user_vm_reboot_handler,
				sock_server, USER_VM_REBOOT);
	} else {
		register_command_handler(socket_req_system_shutdown_user_vm_handler,
						sock_server, REQ_SYS_SHUTDOWN);
	}
	return ret;
}
/* TODO: will refine the name of init_uart_channel_devs_and_shutdown_commands */
int init_uart_channel_devs_and_shutdown_commands(bool service_vm, char *uart_dev_name)
{
	int ret = 0;
	struct channel_dev *c_dev;

	channel = init_uart_channel(life_conf.vm_name);
	if (channel == NULL)
		return -1;
	/**
	 * Open one or more uart channel for lifecycle manager in the service VM,
	 * open one uart channel for lifecycle manager in the user VM.
	 */
	if (service_vm) {
		register_command_handler(sync_cmd_handler, channel, SYNC_CMD);
		register_command_handler(req_shutdown_handler, channel, REQ_SYS_SHUTDOWN);
		register_command_handler(req_reboot_handler, channel, REQ_SYS_REBOOT);
		register_command_handler(ack_poweroff_handler, channel, ACK_POWEROFF);
		register_command_handler(ack_timeout_handler, channel, ACK_TIMEOUT);
		register_command_handler(ack_user_vm_shutdown_cmd_handler, channel, ACK_USER_VM_SHUTDOWN);
		register_command_handler(ack_user_vm_reboot_cmd_handler, channel, ACK_USER_VM_REBOOT);

		ret = create_service_vm_uart_channel_dev(uart_dev_name);
		if (ret < 0)
			return ret;
	} else {
		register_command_handler(acked_sync_handler, channel, ACK_SYNC);
		register_command_handler(poweroff_cmd_handler, channel, POWEROFF_CMD);
		register_command_handler(user_vm_shutdown_cmd_handler, channel, USER_VM_SHUTDOWN);
		register_command_handler(user_vm_reboot_cmd_handler, channel, USER_VM_REBOOT);
		register_command_handler(acked_req_shutdown_handler, channel, ACK_REQ_SYS_SHUTDOWN);
		register_command_handler(ack_timeout_default_handler, channel, ACK_TIMEOUT);

		c_dev = create_uart_channel_dev(channel, uart_dev_name, monitor_cmd_dispatch);
		if (c_dev == NULL)
			return -1;
		strncpy(c_dev->name, SERVICE_VM_NAME, CHANNEL_DEV_NAME_MAX - 1U);
		/* TODO: will refine this connect_uart_channel_dev for pre-lauched VM later*/
		pthread_create(&c_dev->listen_thread, NULL, connect_uart_channel_dev, c_dev);
		pthread_create(&c_dev->pool_thread, NULL, poll_and_dispatch_uart_channel_events, c_dev);
	}
	return ret;
}
/**
 * @brief Parse communication channel device type from configuration file
 *
 * @param dev_conf communication channel device configuration
 */
static int parse_cmd_channel_conf(char *dev_conf)
{
	char *channel_name;
	char *saveptr;
	int ret = -1;

	channel_name = strtok_r(dev_conf, ":", &saveptr);

	if (strncmp(channel_name, "tty", sizeof("tty")) == 0)
		ret = 0;
	else
		LOG_WRITE("Invalid channel type in config file\n");

	memcpy(dev_conf, saveptr, strlen(saveptr) + 1);
	return ret;
}
static int start_life_mngr(void)
{
	int ret = 0;

	if (!open_log("/var/log/life_mngr.log")) {
		printf("Open log file failed\r\n");
		return -ENOENT;
	}
	LOG_WRITE("------Lifecycle Manager start----------\n");
	memset(&life_conf, 0x0, sizeof(struct life_mngr_config));
	if (!load_config(LIFE_MNGR_CONFIG_PATH)) {
		LOG_WRITE("Failed to load configuration file\n");
		return -ENOENT;
	}

	if ((ret = parse_cmd_channel_conf(life_conf.dev_names)) < 0)
		return ret;
	if (strncmp("service_vm", life_conf.vm_type, MAX_CONFIG_VALUE_LEN) == 0) {
		if ((ret = init_socket_server_and_shutdown_commands(true)) < 0)
			return ret;
		ret = init_uart_channel_devs_and_shutdown_commands(true, life_conf.dev_names);
	} else if (strncmp("user_vm", life_conf.vm_type, MAX_CONFIG_VALUE_LEN) == 0) {
		if ((ret = init_socket_server_and_shutdown_commands(false)) < 0)
			return ret;
		ret = init_uart_channel_devs_and_shutdown_commands(false, life_conf.dev_names);
	} else {
		LOG_WRITE("Invalid VM type in config file\n");
		close_log();
		return -EINVAL;
	}
	/* Wait all uart channel threads exit */
	wait_uart_channel_devs_threads(channel);
	return ret;
}
static void stop_life_mngr(void)
{
	deinit_uart_channel(channel);
	deinit_socket(sock_server);
	close_log();
}
int main(int argc, char *argv[])
{
	int ret;

	ret = start_life_mngr();
	if (ret < 0) {
		printf("Failed to start lifecycle Manager, ret=%d\n", ret);
		return ret;
	}
	stop_life_mngr();
	if (get_system_shutdown_flag()) {
		do {
			ret = system(POWEROFF);
		} while (ret < 0);
	}
	if (get_vm_reboot_flag()) {
		do {
			ret = system(REBOOT);
		} while (ret < 0);
	}
	return 0;
}

