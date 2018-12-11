/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <sys/un.h>
#include <pthread.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include "mevent.h"
#include "acrn_mngr.h"

/* helpers */
/* Check if @path is a directory, and create if not exist */
static int check_dir(const char *path)
{
	struct stat st;

	if (stat(path, &st)) {
		if (mkdir(path, 0666)) {
			perror(path);
			return -1;
		}
		return 0;
	}

	if (S_ISDIR(st.st_mode))
		return 0;

	pdebug();
	return -1;
}

#define MNGR_SOCK_FMT		"/run/acrn/mngr/%s.%d.socket"
#define MNGR_MAX_HANDLER	8
#define MNGR_MAX_CLIENT		4
#define PATH_LEN	128

#define CLIENT_BUF_LEN  4096

struct mngr_client {
	/* the rest should be invisible for msg_handler */
	struct sockaddr_un addr;
	int fd;
	socklen_t addr_len;
	void *buf;
	int len;		/* buf len */
	LIST_ENTRY(mngr_client) list;
};

struct mngr_handler {
	unsigned id;
	void (*cb) (struct mngr_msg * msg, int client_fd, void *priv);
	void *priv;
	LIST_ENTRY(mngr_handler) list;
};

struct mngr_fd {
	int type;
	int desc;		/* unique int to this descripter */
				/* returned by mngr_open_un */
	LIST_ENTRY(mngr_fd) list;

	/* Unix socket stuff */
	int fd;			/* the unix socket fd */
	struct sockaddr_un addr;
	socklen_t addr_len;

	/* for servet */
	int listening;
	int polling;
	pthread_t listen_thread;	/* for connect/disconnect */
	pthread_t poll_thread;	/* poll requests */

	/* a server can have many client connection */
	LIST_HEAD(client_list, mngr_client) client_head;	/* clients for this server */
	pthread_mutex_t client_mtx;
	int num_client;

	/* handlers */
	LIST_HEAD(handler_list, mngr_handler) handler_head;	/* clients for this server */
	pthread_mutex_t handler_mtx;
};

static struct mngr_client *mngr_client_new(struct mngr_fd *mfd)
{
	struct mngr_client *client;

	client = calloc(1, sizeof(*client));
	if (!client) {
		pdebug();
		goto alloc_client;
	}

	client->buf = calloc(1, CLIENT_BUF_LEN);
	if (!client->buf) {
		pdebug();
		goto alloc_buf;
	}

	client->addr_len = sizeof(client->addr);
	client->fd =
	    accept(mfd->fd, (struct sockaddr *)&client->addr,
		   &client->addr_len);
	if (client->fd < 0) {
		pdebug();
		goto accept_con;
	}

	pthread_mutex_lock(&mfd->client_mtx);
	LIST_INSERT_HEAD(&mfd->client_head, client, list);
	pthread_mutex_unlock(&mfd->client_mtx);

	return client;

 accept_con:
	free(client->buf);
	client->buf = NULL;
 alloc_buf:
	free(client);
 alloc_client:
	return NULL;
}

static void mngr_client_free_res(struct mngr_client *client)
{
	close(client->fd);
	client->fd = -1;
	free(client->buf);
	client->buf = NULL;
	free(client);
}

static void mngr_client_free(struct mngr_fd *mfd, struct mngr_client *client)
{
	pthread_mutex_lock(&mfd->client_mtx);
	LIST_REMOVE(client, list);
	pthread_mutex_unlock(&mfd->client_mtx);

	mngr_client_free_res(client);
}

static LIST_HEAD(mngr_fd_list, mngr_fd) mngr_fd_head;
static pthread_mutex_t mngr_fd_mtx = PTHREAD_MUTEX_INITIALIZER;

static void *server_listen_func(void *arg)
{
	struct mngr_fd *mfd = arg;
	struct mngr_client *client;

	printf("Listening %d...\n", mfd->desc);
	while (mfd->listening) {
		/* wait connection */
		if (mfd->num_client >= MNGR_MAX_CLIENT) {
			usleep(500000);
			continue;
		}

		client = mngr_client_new(mfd);
		if (!client) {
			usleep(500000);
			continue;
		}
		printf("Connected:%d\n", client->fd);
		mfd->num_client++;
	}
	printf("Stop listening %d...\n", mfd->desc);
	return NULL;
}

static int server_parse_buf(struct mngr_fd *mfd, struct mngr_client *client)
{
	struct mngr_msg *msg;
	struct mngr_handler *handler;
	size_t p = 0;
	int handled = 0;

	if (client->len < sizeof(struct mngr_msg))
		return -1;

	do {
		msg = client->buf + p;

		/* do we out-of-boundary? */
		if (p + sizeof(struct mngr_msg) > client->len) {
			pdebug();
			break;
		}

		LIST_FOREACH(handler, &mfd->handler_head, list) {
			if (msg->magic != MNGR_MSG_MAGIC)
				return -1;
			if (handler->id != msg->msgid)
				continue;

			handler->cb(msg, client->fd, handler->priv);
			handled = 1;
			break;
		}
		p += sizeof(struct mngr_msg);
	} while (p < client->len);

	if (!handled)
		fprintf(stderr, "Unknown message id: %d\n", msg->msgid);

	return 0;
}

static void *server_poll_func(void *arg)
{
	struct mngr_fd *mfd = arg;
	struct mngr_client *client;
	fd_set rfd;
	int max_fd = 0;
	struct timeval timeout;
	struct mngr_client *poll_client[MNGR_MAX_CLIENT];
	int nfd, i;

	printf("polling %d...\n", mfd->desc);
	while (mfd->polling) {
		max_fd = 0;
		nfd = 0;
		pthread_mutex_lock(&mfd->client_mtx);
		FD_ZERO(&rfd);
		LIST_FOREACH(client, &mfd->client_head, list) {
			FD_SET(client->fd, &rfd);
			poll_client[nfd] = client;
			nfd++;
			if (client->fd > max_fd)
				max_fd = client->fd;
		}
		pthread_mutex_unlock(&mfd->client_mtx);

		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;
		select(max_fd + 1, &rfd, NULL, NULL, &timeout);

		for (i = 0; i < nfd; i++) {
			client = poll_client[i];
			if (!FD_ISSET(client->fd, &rfd))
				continue;
			client->len =
			    read(client->fd, client->buf, CLIENT_BUF_LEN);
			if (client->len <= 0) {
				fprintf(stderr, "Disconnect(%d)!\r\n",
					client->fd);
				mngr_client_free(mfd, client);
				mfd->num_client--;
				continue;
			}
			if (client->len == CLIENT_BUF_LEN) {
				fprintf(stderr, "TODO: buf overflow!\r\n");
				continue;
			}

			server_parse_buf(mfd, client);
		}
	}
	printf("Stop polling %d...\n", mfd->desc);

	return NULL;
}

static struct mngr_fd *desc_to_mfd_nolock(int val)
{
	struct mngr_fd *fd;
	struct mngr_fd *find = NULL;

	LIST_FOREACH(fd, &mngr_fd_head, list)
	    if (val == fd->desc) {
		find = fd;
		break;
	}

	return find;
}

/* Does this integer number has a mngr_fd behind? */
static struct mngr_fd *desc_to_mfd(int val)
{
	struct mngr_fd *find = NULL;

	pthread_mutex_lock(&mngr_fd_mtx);
	find = desc_to_mfd_nolock(val);
	pthread_mutex_unlock(&mngr_fd_mtx);

	return find;
}

static int create_new_server(const char *name)
{
	struct mngr_fd *mfd;
	int ret;
	char path[128] = { };

	if (snprintf(path, sizeof(path), MNGR_SOCK_FMT, name, getpid()) >= sizeof(path)) {
		printf("WARN: the path is truncated\n");
		return -1;
	}

	mfd = calloc(1, sizeof(*mfd));
	if (!mfd) {
		perror("Alloc struct mngr_fd");
		ret = errno;
		goto alloc_mfd;
	}
	pthread_mutex_init(&mfd->client_mtx, NULL);
	mfd->type = MNGR_SERVER;

	/* Socket stuff */
	unlink(path);
	mfd->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (mfd->fd < 0) {
		pdebug();
		ret = mfd->fd;
		goto sock_err;
	}
	mfd->addr.sun_family = AF_UNIX;
	strncpy(mfd->addr.sun_path, path, sizeof(mfd->addr.sun_path));

	ret = bind(mfd->fd, (struct sockaddr *)&mfd->addr, sizeof(mfd->addr));
	if (ret < 0) {
		pdebug();
		goto bind_err;
	}
	listen(mfd->fd, 1);

	/* create a listen_thread */
	mfd->listening = 1;
	ret =
	    pthread_create(&mfd->listen_thread, NULL, server_listen_func, mfd);
	if (ret < 0) {
		pdebug();
		goto listen_err;
	}
	pthread_setname_np(mfd->listen_thread, "mngr_listen");

	/* create a poll_thread */
	mfd->polling = 1;
	ret = pthread_create(&mfd->poll_thread, NULL, server_poll_func, mfd);
	if (ret < 0) {
		pdebug();
		goto poll_err;
	}
	pthread_setname_np(mfd->poll_thread, "mngr_pull");

	mfd->desc = mfd->fd;
	/* add this to mngr_fd_head */
	pthread_mutex_lock(&mngr_fd_mtx);
	LIST_INSERT_HEAD(&mngr_fd_head, mfd, list);
	pthread_mutex_unlock(&mngr_fd_mtx);

	return mfd->desc;

 poll_err:
	mfd->listening = 0;
	pthread_join(mfd->listen_thread, NULL);
 listen_err:
	unlink(path);
 bind_err:
	close(mfd->fd);
 sock_err:
	free(mfd);
 alloc_mfd:
	return ret;
}

static void close_server(struct mngr_fd *mfd)
{
	struct mngr_client *client, *tclient;
	struct mngr_handler *handler, *thandler;

	shutdown(mfd->fd, SHUT_RDWR);

	mfd->listening = 0;
	pthread_join(mfd->listen_thread, NULL);

	mfd->polling = 0;
	pthread_join(mfd->poll_thread, NULL);

	pthread_mutex_lock(&mfd->client_mtx);
	list_foreach_safe(client, &mfd->client_head, list, tclient) {
		LIST_REMOVE(client, list);
		mngr_client_free_res(client);
	}
	pthread_mutex_unlock(&mfd->client_mtx);

	pthread_mutex_lock(&mfd->handler_mtx);
	list_foreach_safe(handler, &mfd->handler_head, list, thandler) {
		LIST_REMOVE(handler, list);
		free(handler);
	}
	pthread_mutex_unlock(&mfd->handler_mtx);

	unlink(mfd->addr.sun_path);
	close(mfd->fd);

	free(mfd);
}

static int connect_to_server(const char *name)
{
	struct mngr_fd *mfd;
	int ret;
	DIR *dir;
	char *s_name = NULL, *p = NULL;
	struct dirent *entry;

	dir = opendir("/run/acrn/mngr");
	if (!dir) {
		pdebug();
		return -1;
	}

	while ((entry = readdir(dir))) {
		p = strchr(entry->d_name, '.');
		if (!p || p == entry->d_name)
			continue;
		else
			ret = p - entry->d_name;

		if (!strncmp(entry->d_name, name, ret)) {
			s_name = entry->d_name;
			break;
		}
	}

	if (!s_name) {
		pdebug();
		closedir(dir);
		return -1;
	}

	mfd = calloc(1, sizeof(*mfd));
	if (!mfd) {
		perror("Alloc struct mngr_fd");
		ret = errno;
		goto alloc_mfd;
	}

	mfd->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (mfd->fd < 0) {
		printf("%s %d\n", __FUNCTION__, __LINE__);
		ret = -1;
		goto sock_err;
	}

	mfd->addr.sun_family = AF_UNIX;
	ret = snprintf(mfd->addr.sun_path, sizeof(mfd->addr.sun_path),
		 "/run/acrn/mngr/%s", s_name);
	if (ret >= sizeof(mfd->addr.sun_path))
		printf("WARN: %s is truncated\n", s_name);

	ret =
	    connect(mfd->fd, (struct sockaddr *)&mfd->addr, sizeof(mfd->addr));
	if (ret < 0) {
		printf("%s %d\n", __FUNCTION__, __LINE__);
		goto connect_err;
	}

	mfd->desc = mfd->fd;
	/* add this to mngr_fd_head */
	pthread_mutex_lock(&mngr_fd_mtx);
	LIST_INSERT_HEAD(&mngr_fd_head, mfd, list);
	pthread_mutex_unlock(&mngr_fd_mtx);

	closedir(dir);
	return mfd->desc;

 connect_err:
	close(mfd->fd);
 sock_err:
	free(mfd);
 alloc_mfd:
	closedir(dir);
	return ret;
}

static void close_client(struct mngr_fd *mfd)
{
	close(mfd->fd);
	free(mfd);
}

int mngr_open_un(const char *name, int flags)
{
	check_dir("/run/acrn");
	check_dir("/run/acrn/mngr");

	if (!name) {
		pdebug();
		return -1;
	}

	switch (flags) {
	case MNGR_SERVER:
		return create_new_server(name);
	case MNGR_CLIENT:
		return connect_to_server(name);
	default:
		pdebug();
	}

	return -1;
}

void mngr_close(int val)
{
	struct mngr_fd *mfd;

	mfd = desc_to_mfd(val);
	if (!mfd) {
		pdebug();
		return;
	}

	pthread_mutex_lock(&mngr_fd_mtx);
	LIST_REMOVE(mfd, list);
	pthread_mutex_unlock(&mngr_fd_mtx);

	switch (mfd->type) {
	case MNGR_SERVER:
		close_server(mfd);
		break;
	case MNGR_CLIENT:
		close_client(mfd);
		break;
	default:
		pdebug();
	}

}

int mngr_add_handler(int server_fd, unsigned id,
		     void (*cb) (struct mngr_msg * msg, int client_fd,
				 void *param), void *param)
{
	struct mngr_fd *mfd;
	struct mngr_handler *handler;

	mfd = desc_to_mfd(server_fd);
	if (!mfd) {
		pdebug();
		return -1;
	}

	handler = calloc(1, sizeof(*handler));
	if (!handler) {
		pdebug();
		return -1;
	}

	handler->id = id;
	handler->cb = cb;
	handler->priv = param;

	pthread_mutex_lock(&mfd->handler_mtx);
	LIST_INSERT_HEAD(&mfd->handler_head, handler, list);
	pthread_mutex_unlock(&mfd->handler_mtx);

	return 0;
}

int mngr_send_msg(int fd, struct mngr_msg *req, struct mngr_msg *ack,
		  unsigned timeout)
{
	int socket_fd = fd;
	fd_set rfd, wfd;
	struct timeval t;
	int ret;

	if (!req) {
		printf("%s %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	t.tv_sec = timeout;
	t.tv_usec = 0;

	FD_ZERO(&rfd);
	FD_ZERO(&wfd);
	FD_SET(socket_fd, &rfd);
	FD_SET(socket_fd, &wfd);

	if (timeout)
		select(socket_fd + 1, NULL, &wfd, NULL, &t);
	else
		select(socket_fd + 1, NULL, &wfd, NULL, NULL);

	if (!FD_ISSET(socket_fd, &wfd)) {
		printf("%s %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	ret = write(socket_fd, req, sizeof(struct mngr_msg));
	if (ret != sizeof(struct mngr_msg)) {
		printf("%s %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	if (!ack)
		return 0;

	if (timeout)
		select(socket_fd + 1, &rfd, NULL, NULL, &t);
	else
		select(socket_fd + 1, &rfd, NULL, NULL, NULL);

	if (!FD_ISSET(socket_fd, &rfd))
		return 0;

	ret = read(socket_fd, ack, sizeof(struct mngr_msg));

	return ret;
}
