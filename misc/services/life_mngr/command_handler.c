/*
 * Copyright (C)2021-2022 Intel Corporation.
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
#include "uart.h"
#include "uart_channel.h"
#include "command.h"
#include "socket.h"
#include "command_handler.h"
#include "log.h"
#include "config.h"

bool system_shutdown_flag;
bool get_system_shutdown_flag(void)
{
	return system_shutdown_flag;
}

bool user_vm_reboot_flag;
bool system_reboot_flag;
bool system_reboot_request_flag;
bool get_vm_reboot_flag(void)
{
	return user_vm_reboot_flag | system_reboot_flag;
}

/**
 * @brief check whether all acrn-dm instance have been exit or not
 *
 * @return true all acrn-dm instance have been exit
 * @return false at least one acrn-dm exist
 */
static bool wait_post_vms_shutdown(void)
{
	FILE *fp = NULL;
	char command[64], buf[8];
	char *endptr, *ret_str;
	long val;
	int check_time = SHUTDOWN_TIMEOUT/5;
	bool all_done = false;

	snprintf(command, sizeof(command), "pgrep -u root -f acrn-dm | wc -l");
	do {
		fp = popen(command, "r");
		ret_str = fgets(buf, sizeof(buf), fp);
		if (ret_str == NULL)
			LOG_WRITE("Failed to check acrn-dm process\n");
		val = strtol(buf, &endptr, 10) - 1;
		if (val == 0) {
			all_done = true;
			pclose(fp);
			break;
		}
		check_time--;
		LOG_PRINTF("Wait post launched VMs shutdown check_time:%d, Running VM num:%ld\n",
				check_time, val);
		pclose(fp);
		sleep(5);
	} while (check_time > 0);
	return all_done;
}
static void start_system_reboot(void)
{
	static bool platform_reboot;

	if (is_uart_channel_connection_list_empty(channel) && (!platform_reboot)) {
		platform_reboot = true;
		LOG_WRITE("UART connection list is empty, will trigger system reboot\n");
		close_socket(sock_server);
		stop_listen_uart_channel_dev(channel);
		if (wait_post_vms_shutdown()) {
			LOG_WRITE("Service VM starts to reboot.\n");
			system_reboot_flag = true;
		} else {
			LOG_WRITE("Some User VMs failed to power off, cancelled the platform reboot process.\n");
		}
	}
}
static void start_system_shutdown(void)
{
	static bool platform_shutdown;

	if (is_uart_channel_connection_list_empty(channel) && (!platform_shutdown)) {
		platform_shutdown = true;
		LOG_WRITE("UART connection list is empty, will trigger shutdown system\n");
		close_socket(sock_server);
		stop_listen_uart_channel_dev(channel);
		if (wait_post_vms_shutdown()) {
			LOG_WRITE("Service VM starts to power off.\n");
			system_shutdown_flag = true;
		} else {
			LOG_WRITE("Some User VMs failed to power off, cancelled the platform shutdown process.\n");
		}
	}
}

static int send_socket_ack(void *arg, int fd, char *ack)
{
	int ret = 0;
	struct socket_dev *sock = (struct socket_dev *)arg;
	struct socket_client *client = NULL;

	client = find_socket_client(sock, fd);
	if (client == NULL)
		return -1;

	LOG_PRINTF("Receive shutdown request from unix socket, fd=%d\n", client->fd);
	memset(client->buf, 0, CLIENT_BUF_LEN);
	memcpy(client->buf, ack, strlen(ack));
	client->len = strlen(ack);
	ret = write_socket_char(client);
	LOG_PRINTF("Send acked message to unix socket, message=%s\n", ack);
	return ret;
}
int socket_req_shutdown_service_vm_handler(void *arg, int fd)
{
	int ret;

	usleep(LISTEN_INTERVAL + SECOND_TO_US);
	ret = send_socket_ack(arg, fd, ACK_REQ_SYS_SHUTDOWN);
	if (ret < 0)
		return 0;
	start_all_uart_channel_dev_resend(channel, POWEROFF_CMD, VM_SHUTDOWN_RETRY_TIMES);
	notify_all_connected_uart_channel_dev(channel, POWEROFF_CMD);
	start_system_shutdown();
	return 0;
}
static int req_user_vm_shutdown_reboot(void *arg, int fd, char *msg, char *ack_msg)
{
	int ret;
	struct channel_dev *c_dev = NULL;
	struct socket_dev *sock = (struct socket_dev *)arg;
	struct socket_client *client = NULL;

	usleep(LISTEN_INTERVAL + SECOND_TO_US);
	client = find_socket_client(sock, fd);
	if (client == NULL)
		return -1;

	c_dev = find_uart_channel_dev_by_name(channel, client->name);
	if (c_dev == NULL) {
		(void) send_socket_ack(arg, fd, USER_VM_DISCONNECT);
		LOG_PRINTF("Failed to fail to find uart device to communicate with user VM (%s)\n",
					client->name);
		return 0;
	}
	ret = send_socket_ack(arg, fd, ack_msg);
	if (ret < 0) {
		LOG_WRITE("Failed to send ACK by socket\n");
		return 0;
	}
	LOG_PRINTF("Foward (%s) to user VM (%s) by UART\n", msg, c_dev->name);
	start_uart_channel_dev_resend(c_dev, msg, MIN_RESEND_TIME);
	ret = send_message_by_uart(c_dev->uart_device, msg, strlen(msg));
	if (ret < 0)
		LOG_PRINTF("Failed to foward (%s) to user VM by UART\n", msg);
	return ret;
}
int socket_req_user_vm_shutdown_handler(void *arg, int fd)
{
	return req_user_vm_shutdown_reboot(arg, fd, USER_VM_SHUTDOWN, ACK_REQ_USER_VM_SHUTDOWN);
}
int socket_req_user_vm_reboot_handler(void *arg, int fd)
{
	return req_user_vm_shutdown_reboot(arg, fd, USER_VM_REBOOT, ACK_REQ_USER_VM_REBOOT);
}
int socket_req_system_shutdown_user_vm_handler(void *arg, int fd)
{
	int ret;
	struct channel_dev *c_dev = NULL;

	usleep(LISTEN_INTERVAL + SECOND_TO_US);
	c_dev = (struct channel_dev *)LIST_FIRST(&channel->tty_conn_head);
	if (c_dev == NULL) {
		(void) send_socket_ack(arg, fd, USER_VM_DISCONNECT);
		LOG_WRITE("User VM is disconnect\n");
		return 0;
	}

	ret = send_socket_ack(arg, fd, ACK_REQ_SYS_SHUTDOWN);
	if (ret < 0) {
		LOG_WRITE("Failed to send ACK by socket\n");
		return 0;
	}
	LOG_WRITE("Foward shutdown req to service VM by UART\n");
	start_uart_channel_dev_resend(c_dev, REQ_SYS_SHUTDOWN, MIN_RESEND_TIME);
	ret = send_message_by_uart(c_dev->uart_device, REQ_SYS_SHUTDOWN, strlen(REQ_SYS_SHUTDOWN));
	if (ret < 0)
		LOG_WRITE("Failed to foward system shutdown request to service VM by UART\n");
	return ret;
}

static int is_allowed_s5_channel_dev(struct life_mngr_config *conf, struct channel_dev *c_dev)
{
	return strncmp(get_allow_s5_config(conf), get_uart_dev_path(c_dev->uart_device),
					TTY_PATH_MAX);
}

static int is_allowed_sysreboot_channel_dev(struct life_mngr_config *conf, struct channel_dev *c_dev)
{
	return strncmp(get_allow_sysreboot_config(conf), get_uart_dev_path(c_dev->uart_device),
					TTY_PATH_MAX);
}

/**
 * @brief The handler of sync command of lifecycle manager in service VM
 *
 * @param arg uart channel device instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int sync_cmd_handler(void *arg, int fd)
{
	struct channel_dev *c_dev = NULL;
	struct uart_channel *c = (struct uart_channel *)arg;

	c_dev = find_uart_channel_dev(c, fd);
	if (c_dev == NULL)
		return 0;

	(void)send_message_by_uart(c_dev->uart_device, ACK_SYNC, strlen(ACK_SYNC));
	LOG_PRINTF("Receive sync message from user VM (%s), start to talk.\n",
		c_dev->name);
	usleep(2 * WAIT_RECV);
	return 0;
}
int req_reboot_handler(void *arg, int fd)
{
	int ret;
	struct channel_dev *c_dev = NULL;
	struct uart_channel *c = (struct uart_channel *)arg;

	c_dev = find_uart_channel_dev(c, fd);
	if (c_dev == NULL)
		return 0;

	if (is_allowed_sysreboot_channel_dev(&life_conf, c_dev)) {
		LOG_PRINTF("The user VM (%s) is not allowed to trigger system reboot\n",
			c_dev->name);
		return 0;
	}
	LOG_PRINTF("Receive reboot request from user VM (%s)\n", c_dev->name);
	ret = send_message_by_uart(c_dev->uart_device, ACK_REQ_SYS_REBOOT,
								strlen(ACK_REQ_SYS_REBOOT));
	if (ret < 0)
		LOG_WRITE("Sending a reboot acknowledgement message to user VM failed.\n");
	system_reboot_request_flag = true;
	usleep(SECOND_TO_US);
	LOG_PRINTF("Send acked shutdown request message to user VM (%s)\n", c_dev->name);
	start_all_uart_channel_dev_resend(c, POWEROFF_CMD, VM_SHUTDOWN_RETRY_TIMES);
	notify_all_connected_uart_channel_dev(c, POWEROFF_CMD);
	usleep(2 * WAIT_RECV);
	return ret;
}

/**
 * @brief The handler of system shutdown request command of lifecycle manager in service VM
 *
 * @param arg uart channel device instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int req_shutdown_handler(void *arg, int fd)
{
	int ret;
	struct channel_dev *c_dev = NULL;
	struct uart_channel *c = (struct uart_channel *)arg;

	c_dev = find_uart_channel_dev(c, fd);
	if (c_dev == NULL)
		return 0;

	if (is_allowed_s5_channel_dev(&life_conf, c_dev)) {
		LOG_PRINTF("The user VM (%s) is not allowed to trigger system shutdown\n",
			c_dev->name);
		return 0;
	}
	LOG_PRINTF("Receive shutdown request from user VM (%s)\n", c_dev->name);
	ret = send_message_by_uart(c_dev->uart_device, ACK_REQ_SYS_SHUTDOWN,
								strlen(ACK_REQ_SYS_SHUTDOWN));
	if (ret < 0)
		LOG_WRITE("Sending a shutdown acknowledgement message to user VM failed.\n");
	usleep(SECOND_TO_US);
	LOG_PRINTF("Send acked shutdown request message to user VM (%s)\n", c_dev->name);
	start_all_uart_channel_dev_resend(c, POWEROFF_CMD, VM_SHUTDOWN_RETRY_TIMES);
	notify_all_connected_uart_channel_dev(c, POWEROFF_CMD);
	usleep(2 * WAIT_RECV);
	return ret;
}

/**
 * @brief The handler of acked poweroff command of lifecycle manager in service VM
 *
 * @param arg uart channel instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int ack_poweroff_handler(void *arg, int fd)
{
	struct channel_dev *c_dev = NULL;
	struct uart_channel *c = (struct uart_channel *)arg;

	c_dev = find_uart_channel_dev(c, fd);
	if (c_dev == NULL)
		return 0;
	LOG_PRINTF("Receive poweroff ACK from user VM (%s)\n", c_dev->name);
	stop_uart_channel_dev_resend(c_dev);
	disconnect_uart_channel_dev(c_dev, c);
	usleep(WAIT_USER_VM_POWEROFF);
	if (system_reboot_request_flag) {
		start_system_reboot();
	} else {
		start_system_shutdown();
	}
	return 0;
}
/**
 * @brief The handler of ACK timeout command of lifecycle manager in service VM
 *
 * @param arg uart channel instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int ack_timeout_handler(void *arg, int fd)
{
	struct channel_dev *c_dev = NULL;
	struct uart_channel *c = (struct uart_channel *)arg;

	c_dev = find_uart_channel_dev(c, fd);
	if (c_dev == NULL)
		return 0;
	if (strncmp(c_dev->resend_buf, POWEROFF_CMD, strlen(POWEROFF_CMD)) == 0)
		ack_poweroff_handler(arg, fd);
	else
		stop_uart_channel_dev_resend(c_dev);
	return 0;
}
static int ack_user_vm_cmd(void *arg, int fd, char *ack_msg)
{
	struct channel_dev *c_dev = NULL;
	struct uart_channel *c = (struct uart_channel *)arg;

	c_dev = find_uart_channel_dev(c, fd);
	if (c_dev == NULL)
		return 0;
	LOG_PRINTF("Receive (%s) from user VM (%s)\n", ack_msg, c_dev->name);
	stop_uart_channel_dev_resend(c_dev);
	return 0;
}
int ack_user_vm_shutdown_cmd_handler(void *arg, int fd)
{
	return ack_user_vm_cmd(arg, fd, ACK_USER_VM_SHUTDOWN);
}

int ack_user_vm_reboot_cmd_handler(void *arg, int fd)
{
	return ack_user_vm_cmd(arg, fd, ACK_USER_VM_REBOOT);
}
/**
 * @brief The handler of acked sync command of lifecycle manager in user VM
 *
 * @param arg uart channel device instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int acked_sync_handler(void *arg, int fd)
{
	struct channel_dev *c_dev = NULL;
	struct uart_channel *c = (struct uart_channel *)arg;

	c_dev = find_uart_channel_dev(c, fd);
	if (c_dev == NULL)
		return 0;
	LOG_WRITE("Receive acked sync message from service VM\n");
	return 0;
}

/**
 * @brief The handler of acked system shutdown request command of lifecycle manager in user VM
 *
 * @param arg uart channel instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int acked_req_shutdown_handler(void *arg, int fd)
{
	struct channel_dev *c_dev = NULL;
	struct uart_channel *c = (struct uart_channel *)arg;

	c_dev = find_uart_channel_dev(c, fd);
	if (c_dev == NULL)
		return 0;
	stop_uart_channel_dev_resend(c_dev);
	LOG_WRITE("Receive shutdown request ACK from service VM\n");
	return 0;
}
static int user_vm_shutdown_reboot(struct uart_channel *c, int fd, char *ack, bool reboot)
{
	int ret;
	struct channel_dev *c_dev = NULL;

	c_dev = find_uart_channel_dev(c, fd);
	if (c_dev == NULL)
		return 0;

	ret = send_message_by_uart(c_dev->uart_device, ack, strlen(ack));
	if (ret < 0) {
		LOG_PRINTF("Failed to send (%s) to service VM\n", ack);
	}
	disconnect_uart_channel_dev(c_dev, c);
	usleep(2 * WAIT_RECV);
	close_socket(sock_server);
	if (reboot) {
		user_vm_reboot_flag = true;
	} else {
		system_shutdown_flag = true;
	}
	return 0;
}
/**
 * @brief The handler of poweroff command of lifecycle manager in user VM
 *
 * @param arg uart channel device instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int poweroff_cmd_handler(void *arg, int fd)
{

	struct uart_channel *c = (struct uart_channel *)arg;
	(void) user_vm_shutdown_reboot(c, fd, ACK_POWEROFF, false);
	return 0;
}
int user_vm_shutdown_cmd_handler(void *arg, int fd)
{
	struct uart_channel *c = (struct uart_channel *)arg;
	(void) user_vm_shutdown_reboot(c, fd, ACK_USER_VM_SHUTDOWN, false);
	return 0;
}
int user_vm_reboot_cmd_handler(void *arg, int fd)
{
	struct uart_channel *c = (struct uart_channel *)arg;
	(void) user_vm_shutdown_reboot(c, fd, ACK_USER_VM_REBOOT, true);
	return 0;
}
/**
 * @brief The handler of ACK timeout command of lifecycle manager in user VM
 *
 * @param arg uart channel instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int ack_timeout_default_handler(void *arg, int fd)
{
	struct channel_dev *c_dev = NULL;
	struct uart_channel *c = (struct uart_channel *)arg;

	c_dev = find_uart_channel_dev(c, fd);
	if (c_dev == NULL)
		return 0;
	stop_uart_channel_dev_resend(c_dev);
	disconnect_uart_channel_dev(c_dev, c);
	close_socket(sock_server);
	LOG_PRINTF("Failed to receive ACK message from service VM (fd = %d)\n", fd);
	return 0;
}
