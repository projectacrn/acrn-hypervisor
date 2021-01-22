/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/cdefs.h>

#define RESERVED_SOCKET_PREFIX "/tmp/"
#define SOCKET_PATH_MAX 128

int create_socket_server(const char *name, int type);

int socket_local_client(const char *name, const size_t len, int type);
ssize_t send_fd(int sockfd, const void *data, size_t len, int fd);
ssize_t recv_fd(int sockfd, void *data, size_t len, int *out_fd);

#endif
