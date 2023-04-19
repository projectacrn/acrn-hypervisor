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
#include <sys/queue.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "socket.h"
#include "log.h"
#include "list.h"


static int setup_and_listen_unix_socket(const char *sock_path, int num)
{
	struct sockaddr_un s_un;
	int sock_fd, ret;

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0)
		goto err;
	s_un.sun_family = AF_UNIX;
	ret = snprintf(s_un.sun_path, sizeof(s_un.sun_path), "%s",
			sock_path);
	if (ret < 0 || ret >= sizeof(s_un.sun_path))
		goto err_close_socket;

	if (bind(sock_fd, (struct sockaddr *)&s_un, sizeof(s_un)) < 0)
		goto err_close_socket;
	if (listen(sock_fd, num) < 0)
		goto err_close_socket;
	LOG_PRINTF("Start to listen:%s\r\n", sock_path);
	return sock_fd;
err_close_socket:
	close(sock_fd);
err:
	return -1;
}
static void free_socket_client(struct socket_dev *sock, struct socket_client *client)
{
	pthread_mutex_lock(&sock->client_mtx);
	LIST_REMOVE(client, list);
	pthread_mutex_unlock(&sock->client_mtx);

	close(client->fd);
	client->fd = -1;
	free(client);
}

int write_socket_char(struct socket_client *client)
{
	struct msghdr msg;
	struct iovec iov[1];
	int ret;
	char control[CMSG_SPACE(sizeof(int))];
	struct cmsghdr *cmsg;

	memset(&msg, 0, sizeof(msg));
	memset(control, 0, sizeof(control));

	iov[0].iov_base = (void *)client->buf;
	iov[0].iov_len = client->len;

	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	memcpy(CMSG_DATA(cmsg), &client->fd, sizeof(int));

	do {
		ret = sendmsg(client->fd, &msg, 0);
	} while (ret < 0 && errno == EINTR);

	return ret;
}
static void parse_socket_client_name(struct socket_client *client)
{
	char *saveptr;

	(void) strtok_r(client->buf, ":", &saveptr);
	if (strlen(saveptr) > 0) {
		strncpy(client->name, saveptr, SOCKET_CLIENT_NAME_MAX - 1U);
		LOG_PRINTF("Socket client name:%s\n", client->name);
	}
}
int read_socket_char(struct socket_dev *sock, struct socket_client *client)
{
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	char buf[CMSG_SPACE(sizeof(int))];
	int fdflags_recvmsg = MSG_CMSG_CLOEXEC;

	memset(&msg, 0, sizeof(msg));
	memset(client->buf, '\0', CLIENT_BUF_LEN);
	iov.iov_base = client->buf;
	iov.iov_len = CLIENT_BUF_LEN;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));

	client->len = recvmsg(client->fd, &msg, fdflags_recvmsg);
	if (client->len <= 0) {
		LOG_PRINTF("Socket Disconnect(%d)!\r\n", client->fd);
		free_socket_client(sock, client);
		sock->num_client--;
		return -1;
	}
	if (client->len == CLIENT_BUF_LEN) {
		LOG_WRITE("Socket buf overflow!\r\n");
		return -1;
	}
	for (unsigned int i = 0U; i < CLIENT_BUF_LEN; i++)
		if (client->buf[i] == 0xa)
			client->buf[i] = '\0';
	LOG_PRINTF("Receive data:(%s)\n", client->buf);
	parse_socket_client_name(client);
	if (sock->data_handler != NULL)
		sock->data_handler(client->buf, client->fd);
	return 0;
}
static struct socket_client *new_socket_client(struct socket_dev *sock)
{
	struct socket_client *client;

	client = calloc(1, sizeof(*client));
	if (!client) {
		LOG_PRINTF("%s: failed to allocate memory for client\n",
					__func__);
		goto alloc_client;
	}

	client->addr_len = sizeof(client->addr);
	client->fd =
	    accept(sock->sock_fd, (struct sockaddr *)&client->addr,
		   &client->addr_len);
	if (client->fd < 0) {
		if (sock->listening)
			LOG_PRINTF("%s: Failed to accept from fd %d, err: %s\n",
					__func__, sock->sock_fd, strerror(errno));
		goto accept_con;
	}

	pthread_mutex_lock(&sock->client_mtx);
	LIST_INSERT_HEAD(&sock->client_head, client, list);
	pthread_mutex_unlock(&sock->client_mtx);

	return client;

 accept_con:
	free(client);
 alloc_client:
	return NULL;
}
static void *listen_socket_client(void *arg)
{
	struct socket_dev *sock = (struct socket_dev *)arg;
	struct socket_client *client;

	LOG_PRINTF("Socket Listening %d...\n", sock->sock_fd);
	while (sock->listening) {
		/* wait connection */
		if (sock->num_client >= SOCKET_MAX_CLIENT) {
			usleep(500000);
			continue;
		}

		client = new_socket_client(sock);
		if (!client) {
			usleep(500000);
			continue;
		}
		LOG_PRINTF("Socket Connected:%d\n", client->fd);
		sock->num_client++;
	}
	LOG_PRINTF("Stop listening %d...\n", sock->sock_fd);
	return NULL;
}
static void *socket_poll_events(void *arg)
{
	struct socket_dev *sock = (struct socket_dev *)arg;
	struct socket_client *client;
	fd_set rfd;
	int max_fd = 0;
	struct timeval timeout;
	struct socket_client *poll_client[SOCKET_MAX_CLIENT];
	int nfd, i;

	LOG_PRINTF("Socket polling %d...\n", sock->sock_fd);
	while (sock->polling) {
		max_fd = 0;
		nfd = 0;
		pthread_mutex_lock(&sock->client_mtx);
		FD_ZERO(&rfd);
		LIST_FOREACH(client, &sock->client_head, list) {
			FD_SET(client->fd, &rfd);
			poll_client[nfd] = client;
			nfd++;
			if (client->fd > max_fd)
				max_fd = client->fd;
		}
		pthread_mutex_unlock(&sock->client_mtx);

		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;
		select(max_fd + 1, &rfd, NULL, NULL, &timeout);

		for (i = 0; i < nfd; i++) {
			client = poll_client[i];
			if (!FD_ISSET(client->fd, &rfd))
				continue;

			if (read_socket_char(sock, client) < 0)
				continue;
		}
	}
	LOG_PRINTF("Socket Stop polling %d...\n", sock->sock_fd);

	return NULL;
}
struct socket_client *find_socket_client(struct socket_dev *sock, int fd)
{
	struct socket_client *client = NULL;
	pthread_mutex_lock(&sock->client_mtx);
	LIST_FOREACH(client, &sock->client_head, list) {
		if (client->fd == fd)
			break;
	}
	pthread_mutex_unlock(&sock->client_mtx);
	return client;
}
int open_socket(struct socket_dev *sock, data_handler_f *fn)
{
	sock->listening = true;
	sock->polling = true;
	sock->data_handler = fn;
	pthread_mutex_init(&sock->client_mtx, NULL);
	unlink(sock->unix_sock_path);
	sock->sock_fd = setup_and_listen_unix_socket(sock->unix_sock_path, SOCKET_MAX_CLIENT);
	if (sock->sock_fd < 0)
		return -1;
	pthread_create(&sock->listen_thread, NULL, listen_socket_client, sock);
	pthread_create(&sock->connect_thread, NULL, socket_poll_events, sock);
	return 0;
}

void close_socket(struct socket_dev *sock)
{
	struct socket_client *client, *tclient;

	sock->listening = false;
	sock->polling = false;
	shutdown(sock->sock_fd, SHUT_RDWR);

	pthread_join(sock->listen_thread, NULL);
	pthread_join(sock->connect_thread, NULL);

	pthread_mutex_lock(&sock->client_mtx);
	list_foreach_safe(client, &sock->client_head, list, tclient) {
		LIST_REMOVE(client, list);
		close(client->fd);
		client->fd = -1;
		free(client);
	}
	pthread_mutex_unlock(&sock->client_mtx);

	close(sock->sock_fd);
	unlink(sock->unix_sock_path);
}
struct socket_dev *init_socket(char *path)
{
	struct socket_dev *sock;

	sock = calloc(1, sizeof(*sock));
	if (!sock) {
		LOG_PRINTF("%s: Failed to allocate memory for socket\n", __func__);
		return NULL;
	}
	memset(sock, 0x0, sizeof(struct socket_dev));
	strncpy(sock->unix_sock_path, path, UNIX_SOCKET_PATH_MAX - 1);
	return sock;
}
void deinit_socket(struct socket_dev *sock)
{
	if (sock != NULL)
		free(sock);
}