/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Copyright (C) 2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stddef.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "protocol.h"
#include "log_sys.h"

#define SUN_PATH_MAX \
	(sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))

/* Documented in header file. */
static int socket_make_sockaddr_un(const char *name, const size_t len,
		struct sockaddr_un *p_addr, socklen_t *alen)
{
	size_t socket_len;

	socket_len = strlen(RESERVED_SOCKET_PREFIX);
	if (socket_len >= SUN_PATH_MAX)
		return -1;
	strncpy(p_addr->sun_path, RESERVED_SOCKET_PREFIX, socket_len + 1);
	if (len >= (SUN_PATH_MAX - socket_len))
		return -1;
	strncat(p_addr->sun_path, name, len);

	p_addr->sun_family = AF_LOCAL;
	*alen = len + socket_len +
		offsetof(struct sockaddr_un, sun_path) + 1;
	return 0;
}

/**
 * connect to peer named "name" on fd
 * returns same fd or -1 on error.
 * fd is not closed on error. that's your job.
 *
 */
static int socket_local_client_connect(int fd, const char *name,
		const size_t len)
{
	struct sockaddr_un addr;
	socklen_t alen;
	int err;

	err = socket_make_sockaddr_un(name, len, &addr, &alen);

	if (err < 0)
		goto error;

	if (connect(fd, (struct sockaddr *) &addr, alen) < 0) {
		LOGE("connect to usercrashd failed ,error (%s)\n",
				strerror(errno));
		goto error;
	}

	return fd;

error:
	return -1;
}

/**
 * connect to peer named "name"
 * returns fd or -1 on error
 */
int socket_local_client(const char *name, const size_t len, int type)
{
	int s;

	s = socket(AF_LOCAL, type, 0);
	if (s < 0)
		return -1;

	if (socket_local_client_connect(s, name, len) < 0) {
		close(s);
		return -1;
	}

	return s;
}

static int socket_bind(int fd, const char *name)
{
	struct sockaddr_un addr;
	socklen_t alen;
	size_t name_len;

	addr.sun_family = AF_UNIX;
	name_len = strnlen(name, SOCKET_PATH_MAX);
	if (name_len >= SUN_PATH_MAX)
		return -1;
	*(char *)mempcpy(addr.sun_path, name, name_len) = '\0';
	unlink(addr.sun_path);
	alen = strnlen(addr.sun_path, SUN_PATH_MAX) + sizeof(addr.sun_family);

	if (bind(fd, (struct sockaddr *)&addr, alen) == -1)
		return -1;

	return fd;
}

int create_socket_server(const char *name, int type)
{
	int err;
	int fd;

	fd = socket(AF_UNIX, type, 0);
	if (fd < 0)
		return -1;

	err = socket_bind(fd, name);

	if (err < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

ssize_t send_fd(int sockfd, const void *data, size_t len, int fd)
{
	char cmsg_buf[CMSG_SPACE(sizeof(int))];
	struct msghdr msg;
	struct iovec iov;
	struct cmsghdr *cmsg;

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = (void *)data;
	iov.iov_len = len;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsg_buf;
	msg.msg_controllen = sizeof(cmsg_buf);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	*(int *)(CMSG_DATA(cmsg)) = fd;

	return sendmsg(sockfd, &msg, 0);
}

ssize_t recv_fd(int sockfd, void *data, size_t len, int *out_fd)
{
	char cmsg_buf[CMSG_SPACE(sizeof(int))];
	struct msghdr msg;
	struct iovec iov;
	struct cmsghdr *cmsg;
	ssize_t result;
	bool received_fd;
	int fd;

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = (void *)data;
	iov.iov_len = len;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsg_buf;
	msg.msg_controllen = sizeof(cmsg_buf);
	msg.msg_flags = 0;

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));

	result = recvmsg(sockfd, &msg, 0);
	if (result == -1)
		return -1;

	received_fd = msg.msg_controllen == sizeof(cmsg_buf);
	if (received_fd)
		fd = *(int *)(CMSG_DATA(cmsg));
	else
		return -1;

	if ((msg.msg_flags & MSG_TRUNC) != 0) {
		errno = EFBIG;
		goto fail;
	} else if ((msg.msg_flags & MSG_CTRUNC) != 0) {
		errno = ERANGE;
		goto fail;
	}

	if (out_fd) {
		*out_fd = fd;
	} else if (received_fd) {
		errno = ERANGE;
		goto fail;
	}

	return result;
fail:
	close(fd);
	return -1;
}
