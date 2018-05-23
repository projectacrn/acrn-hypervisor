/*
 * Project Acrn
 * Acrn-dm-monitor
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Author: TaoYuhong <yuhong.tao@intel.com>
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <errno.h>
#include <time.h>
#include "dm.h"
#include "vmmapi.h"
#include "mevent.h"
#include "monitor.h"

/* Data structure and functions for processing received messages */
struct monitor_msg_handle {
	struct vmm_msg msg;
	void (*callback) (struct vmm_msg * msg, struct msg_sender * sender,
			  void *priv);
	void *priv;
	LIST_ENTRY(monitor_msg_handle) list;
};

static LIST_HEAD(mmh_list_struct, monitor_msg_handle) mmh_head;
static pthread_mutex_t mmh_mutex = PTHREAD_MUTEX_INITIALIZER;
static int can_register_handler = 0;	/* Do not allow anyone add his handler, 
					   untill we have added some researved ones */
static int monitor_add_handler(struct monitor_msg_handle *handle)
{
	struct monitor_msg_handle *hp;

	pthread_mutex_lock(&mmh_mutex);

	LIST_FOREACH(hp, &mmh_head, list)
	    if (hp->msg.msgid == handle->msg.msgid) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		pthread_mutex_unlock(&mmh_mutex);
		return -1;
	}
	LIST_INSERT_HEAD(&mmh_head, handle, list);
	pthread_mutex_unlock(&mmh_mutex);

	return 0;
}

int monitor_register_handler(struct vmm_msg *msg,
			    void (*callback) (struct vmm_msg * msg,
					      struct msg_sender * client,
					      void *priv), void *priv)
{
	struct monitor_msg_handle *handle;
	int ret;

	if (!can_register_handler)
		return -1;

	handle = calloc(1, sizeof(struct monitor_msg_handle));
	if (!handle) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		return -1;
	}

	handle->msg.msgid = msg->msgid;
	handle->callback = callback;
	handle->priv = priv;

	ret = monitor_add_handler(handle);
	if (ret)
		free(handle);

	return ret;
}

/* messages handled by monitor */
static int write_msg_to(int fd, void *data, unsigned long timeout_usec)
{
	struct vmm_msg *msg = data;
	fd_set wfd;
	struct timeval timeout;
	int ret = 0;

	if (msg->len < sizeof(struct vmm_msg)) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		return -1;
	}

	if (msg->msgid > MSGID_MAX) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		return -1;
	}

	if (msg->magic != VMM_MSG_MAGIC) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		msg->magic = VMM_MSG_MAGIC;
	}

	msg->timestamp = time(NULL);

	FD_ZERO(&wfd);
	FD_SET(fd, &wfd);
	timeout.tv_sec = 0;
	timeout.tv_usec = timeout_usec;
	select(fd + 1, NULL, &wfd, NULL, &timeout);

	if (FD_ISSET(fd, &wfd))
		ret = write(fd, msg, msg->len);

	return ret;
}

/* MSG_HANDSHAKE, handshake message handler*/
#define TIMEOUT_USEC	100000
static VMM_MSG_STR(handshake_badname, "Error: bad name!");
static VMM_MSG_STR(handshake_ok, "acrn-dm read you request");

static void handshake_acrn_dm(struct vmm_msg *msg, struct msg_sender *sender,
			      void *priv)
{
	struct vmm_msg_handshake *hsk = (void *)msg;
	int ret;

	ret = strnlen(hsk->name, CLIENT_NAME_LEN);
	if (ret >= CLIENT_NAME_LEN) {
		write_msg_to(sender->fd, &handshake_badname, TIMEOUT_USEC);
		return;
	}

	strncpy(sender->name, hsk->name, CLIENT_NAME_LEN);
	sender->broadcast = hsk->broadcast;

	write_msg_to(sender->fd, &handshake_ok, TIMEOUT_USEC);
}

static struct monitor_msg_handle handle_handshake = {
	.msg = {.msgid = MSG_HANDSHAKE},
	.callback = handshake_acrn_dm,
};

/* vm manager can comunicate with dm-monitor, use unix socket,
 * the monitor is the server, and there may have many clients,
 * a client send a message, trigger right msg handler. And msg handler
 * should only reply to message sender.
 */

static struct sockaddr_un monitor_addr;	/* one monitor */
static int monitor_fd;

struct vmm_client {
	/* msg_sender will be seen/modify by msg handler */
	struct msg_sender sender;

	/* the rest should be invisible for msg_handler */
	struct sockaddr_un addr;
	int fd;
	socklen_t addr_len;
	void *buf;
	int len;		/* buf len */
	struct mevent *mev;
	LIST_ENTRY(vmm_client) list;
};

static LIST_HEAD(client_list_struct, vmm_client) client_head;
static int num_client = 0;
static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

static void vmm_client_free_res(struct vmm_client *client)
{
	mevent_delete(client->mev);
	close(client->fd);
	client->fd = -1;
	free(client->buf);
	client->buf = NULL;
	free(client);
}

static void vmm_client_free(struct vmm_client *client)
{
	pthread_mutex_lock(&client_mutex);
	LIST_REMOVE(client, list);
	num_client--;
	pthread_mutex_unlock(&client_mutex);

	vmm_client_free_res(client);
}

static VMM_MSG_STR(unsupported_msgid, "Error: unsupported msgid!");

static int monitor_parse_buf(struct vmm_client *client)
{
	struct vmm_msg *msg;
	struct monitor_msg_handle *handle;
	size_t p = 0;
	int handled = 0;

	if (client->len < sizeof(struct vmm_msg))
		return -1;
	do {
		msg = client->buf + p;

		/* do we out-of-bounary? */
		if (p + msg->len > client->len) {
			fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
			break;
		}

		LIST_FOREACH(handle, &mmh_head, list) {
			if (msg->magic != VMM_MSG_MAGIC)
				return -1;
			if (handle->msg.msgid != msg->msgid)
				continue;
			client->sender.fd = client->fd;
			handle->callback(msg, &client->sender, handle->priv);
			handled = 1;
			break;
		}
		p += msg->len;
	} while (p < client->len);

	if (!handled)
		write_msg_to(client->fd, &unsupported_msgid, TIMEOUT_USEC);

	return 0;
}

static void mevent_read_func(int fd, enum ev_type type, void *param)
{
	struct vmm_client *client = param;

	client->len = read(fd, client->buf, VMM_MSG_MAX_LEN);
	if (client->len <= 0) {
		fprintf(stderr, "Disconnect(%d)!\r\n", client->fd);
		vmm_client_free(client);
		return;
	}

	if (client->len == VMM_MSG_MAX_LEN) {
		fprintf(stderr, "TODO: buf overflow!\r\n");
		return;
	}

	monitor_parse_buf(client);
}

static struct vmm_client *vmm_client_new(void)
{
	struct vmm_client *client;

	client = calloc(1, sizeof(struct vmm_client));
	if (!client) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto alloc_client;
	}
	memset(client, 0, sizeof(struct vmm_client));

	client->buf = calloc(1, VMM_MSG_MAX_LEN);
	if (!client->buf) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto alloc_buf;
	}

	client->addr_len = sizeof(client->addr);
	client->fd =
	    accept(monitor_fd, (struct sockaddr *)&client->addr, &client->addr_len);
	if (client->fd < 0) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto accept_con;
	}

	client->mev =
	    mevent_add(client->fd, EVF_READ, mevent_read_func, client);
	if (!client->mev) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto add_mev;
	}

	pthread_mutex_lock(&client_mutex);
	LIST_INSERT_HEAD(&client_head, client, list);
	num_client++;
	pthread_mutex_unlock(&client_mutex);

	return client;

 add_mev:
	close(client->fd);
	client->fd = -1;
 accept_con:
	free(client->buf);
	client->buf = NULL;
 alloc_buf:
	free(client);
 alloc_client:
	return NULL;
}

int monitor_broadcast(struct vmm_msg *msg)
{
	struct vmm_client *client;
	fd_set wfd;
	int max_fd = 0;
	struct timeval timeout;
	int ret = 0;

	if (msg->len < sizeof(struct vmm_msg)) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		return -1;
	}

	if (msg->msgid > MSGID_MAX) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		return -1;
	}

	if (msg->magic != VMM_MSG_MAGIC) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		msg->magic = VMM_MSG_MAGIC;
	}

	msg->timestamp = time(NULL);

	pthread_mutex_lock(&client_mutex);

	FD_ZERO(&wfd);
	LIST_FOREACH(client, &client_head, list) {
		if (!client->sender.broadcast)
			continue;
		FD_SET(client->fd, &wfd);
		if (client->fd > max_fd)
			max_fd = client->fd;
	}
	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;
	select(max_fd + 1, NULL, &wfd, NULL, &timeout);

	LIST_FOREACH(client, &client_head, list) {
		if (!client->sender.broadcast)
			continue;
		if (FD_ISSET(client->fd, &wfd)) {
			ret = write(client->fd, msg->payload,
				    msg->len - sizeof(struct vmm_msg));
			if (ret < 0)
				continue;
		}
	}

	pthread_mutex_unlock(&client_mutex);
	return 0;
}

/* monitor thread */
static int monitor_running = 1;
static pthread_t monitor_thread;

static void *monitor_server_func(void *arg)
{
	struct vmm_client *client;
	while (monitor_running) {
		client = vmm_client_new();
		if (!client) {
			usleep(10000);
			continue;
		}
		fprintf(stderr, "Connected:%d\r\n", client->fd);
	}

	fprintf(stderr, "%s quit!\r\n", __FUNCTION__);
	return NULL;
}

int monitor_init(struct vmctx *ctx)
{
	int ret;
	char path[128] = { };

	ret = system("mkdir -p /run/acrn/");
	if (ret) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto socket_err;
	}
	memset(&monitor_addr, 0, sizeof(monitor_addr));
	snprintf(path, sizeof(path), "/run/acrn/%s-monitor.socket", vmname);
	unlink(path);
	monitor_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (monitor_fd < 0) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto socket_err;
	}

	monitor_addr.sun_family = AF_UNIX;
	strncpy(monitor_addr.sun_path, path, sizeof(monitor_addr.sun_path));
	ret = bind(monitor_fd, (struct sockaddr *)&monitor_addr, sizeof(monitor_addr));
	if (ret < 0) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto bind_err;
	}

	listen(monitor_fd, 1);
	ret = pthread_create(&monitor_thread, NULL, monitor_server_func, NULL);
	if (ret) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto thread_err;
	}

	/* Messages handled by monitor */
	monitor_add_handler(&handle_handshake);

	__sync_fetch_and_add(&can_register_handler, 1);
	return 0;

 thread_err:
	monitor_thread = 0;
	unlink(path);
 bind_err:
	close(monitor_fd);
 socket_err:
	return -1;
}

void monitor_close(void)
{
	struct vmm_client *client, *pclient;
	if (!monitor_thread)
		return;
	shutdown(monitor_fd, SHUT_RDWR);
	close(monitor_fd);
	monitor_running = 0;
	pthread_join(monitor_thread, NULL);
	unlink(monitor_addr.sun_path);

	/* client buf-mem and fd may be still in use by msg-handler */
	/* which is handled by mevent */
	pthread_mutex_lock(&client_mutex);
	list_foreach_safe(client, &client_head, list, pclient) {
		LIST_REMOVE(client, list);
		vmm_client_free_res(client);
	}
	pthread_mutex_unlock(&client_mutex);
}
