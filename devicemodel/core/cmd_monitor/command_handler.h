/*
 * Copyright (C) 2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _COMMAND_HANDLER_H_
#define _COMMAND_HANDLER_H_

extern struct socket_dev *sock_server;

int user_vm_destroy_handler(void *arg, void *command_para);
int user_vm_blkrescan_handler(void *arg, void *command_para);
#endif
