/*
 * Copyright (C)2021-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _CMD_HANDLER_H_
#define _CMD_HANDLER_H_

#define SEND_RETRY_TIMES 3

extern struct uart_channel *channel;
extern struct socket_dev *sock_server;
/**
 * @brief Get the system shutdown flag
 */
bool get_system_shutdown_flag(void);
/**
 * @brief Get the reboot flag
 */
bool get_vm_reboot_flag(void);
/**
 * @brief The handler of request system shutdown command on socket in service VM
 */
int socket_req_shutdown_service_vm_handler(void *arg, int fd);
/**
 * @brief The handler of request user shutdown command on socket in service VM
 */
int socket_req_user_vm_shutdown_handler(void *arg, int fd);
/**
 * @brief The handler of request user reboot command on socket in service VM
 */
int socket_req_user_vm_reboot_handler(void *arg, int fd);
/**
 * @brief The handler of request system shutdown command on socket in user VM
 */
int socket_req_system_shutdown_user_vm_handler(void *arg, int fd);

/**
 * @brief The handler of sync command of lifecycle manager in service VM
 *
 * @param arg uart channel device instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int sync_cmd_handler(void *arg, int fd);
/**
 * @brief The handler of system shutdown request command of lifecycle manager in service VM
 *
 * @param arg uart channel device instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int req_shutdown_handler(void *arg, int fd);
/**
 * @brief The handler of system reboot request command of lifecycle manager in service VM
 *
 * @param arg uart channel device instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int req_reboot_handler(void *arg, int fd);
/**
 * @brief The handler of acked poweroff command of lifecycle manager in service VM
 *
 * @param arg uart channel instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int ack_poweroff_handler(void *arg, int fd);
/**
 * @brief The handler of poweroff timeout command of lifecycle manager in service VM
 *
 * @param arg uart channel instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int ack_timeout_handler(void *arg, int fd);
/**
 * @brief The handler of ACK user vm shutdown command of
 * lifecycle manager in service VM
 *
 * @param arg uart channel instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int ack_user_vm_shutdown_cmd_handler(void *arg, int fd);
/**
 * @brief The handler of ACK user vm reboot command of
 * lifecycle manager in service VM
 *
 * @param arg uart channel instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int ack_user_vm_reboot_cmd_handler(void *arg, int fd);
/**
 * @brief The handler of acked sync command of lifecycle manager in user VM
 *
 * @param arg uart channel device instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int acked_sync_handler(void *arg, int fd);
/**
 * @brief The handler of acked system shutdown request command of lifecycle manager in user VM
 *
 * @param arg uart channel instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int acked_req_shutdown_handler(void *arg, int fd);
/**
 * @brief The handler of poweroff command of lifecycle manager in user VM
 *
 * @param arg uart channel device instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int poweroff_cmd_handler(void *arg, int fd);
/**
 * @brief The handler of user VM shutdown command of lifecycle manager in user VM
 */
int user_vm_shutdown_cmd_handler(void *arg, int fd);
/**
 * @brief The handler of user VM reboot command of lifecycle manager in user VM
 */
int user_vm_reboot_cmd_handler(void *arg, int fd);
/**
 * @brief The handler of ACK timeout command of lifecycle manager in user VM
 *
 * @param arg uart channel instance
 * @param fd the file directory of the uart which receives message
 * @return indicate this command is handled successful or not
 */
int ack_timeout_default_handler(void *arg, int fd);
#endif
