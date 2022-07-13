/*
 * Copyright (C)2021-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _SOCKET_H_
#define _SOCKET_H_
#include <sys/queue.h>
#include <pthread.h>
#include <sys/un.h>

#define BUFFER_SIZE 16U
#define UNIX_SOCKET_PATH_MAX 256U
#define CLIENT_BUF_LEN 4096U
#define SOCKET_MAX_CLIENT		4
#define SOCKET_CLIENT_NAME_MAX	128U

typedef void data_handler_f(const char *cmd_name, int fd);

struct socket_client {
	char name[SOCKET_CLIENT_NAME_MAX]; /**< channel device name */
	struct sockaddr_un addr;
	int fd;
	socklen_t addr_len;
	char buf[CLIENT_BUF_LEN];
	int len; /* buf len */

	LIST_ENTRY(socket_client) list;
};

struct socket_dev {
	char unix_sock_path[UNIX_SOCKET_PATH_MAX];
	int sock_fd;
	int logfd;

	data_handler_f *data_handler;

	bool listening;
	bool polling;
	pthread_t listen_thread;
	pthread_t connect_thread;

	LIST_HEAD(client_list, socket_client) client_head;        /* clients for this server */
	pthread_mutex_t client_mtx;
	int num_client;
};

/**
 * @brief Send message through unix domain socket server
 */
int write_socket_char(struct socket_client *client);
/**
 * @brief Find socket client instance according to fd
 */
struct socket_client *find_socket_client(struct socket_dev *sock, int fd);
/**
 * @brief Open one unix domain socket server, initialize a socket,
 * create one thread to listen to client, another thread to poll message from client.
 */
int open_socket(struct socket_dev *sock, data_handler_f *fn);
/**
 * @brief Close one unix domain socket server
 */
void close_socket(struct socket_dev *sock);
/**
 * @brief Initialize a socket
 *
 * @param path the socket path
 * @return struct socket_dev* the socket instance
 */
struct socket_dev *init_socket(char *path);
/**
 * @brief Deinit a socket
 *
 * @param sock The pointer of socket instance
 */
void deinit_socket(struct socket_dev *sock);

#endif
