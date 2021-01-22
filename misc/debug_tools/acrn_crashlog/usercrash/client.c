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

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "crash_dump.h"
#include "packet.h"
#include "log_sys.h"
#include "protocol.h"
#include "version.h"

/**
 * Usercrash works as C/S model: usercrash_c works as usercrash client to
 * collect crash logs and information once crash event occurs. For each time,
 * usercrash_c receives 3 params from core_dump and sends connect request event
 * to usercrash_s, then it receives file fd from server to fill crash info into
 * the file. After this work is done, it will notify server that dump work is
 * completed.
 */

/**
 * @sockfd: the socket fd.
 * set_timeout is used to set timeout for the sockfd, in case client is blocked
 * when client cannot receive the data from server, or send data to server
 */
static int set_timeout(int sockfd)
{
	struct timeval timeout = {50, 0};

	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
				sizeof(timeout)) != 0) {
		LOGE("failed to set receive timeout\n");
		return -1;
	}
	if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
				sizeof(timeout)) != 0) {
		LOGE("failed to set send timeout\n");
		return -1;
	}
	return 0;
}

/**
 * @pid: crash process pid.
 * @usercrashd_socket: client socket fd pointer, get the value from
 * usercrashd_connect function.
 * @output_fd: file fd, receives from server to store dump file.
 * @name: crash process name
 * usercrashd_connect is used to connect to server, and get the crash file fd
 * from server
 */
static int usercrashd_connect(int pid, int *usercrashd_socket,
		int *output_fd, const char *name)
{
	int sockfd;
	int tmp_output_fd;
	ssize_t rc;
	int flags;
	struct crash_packet packet = {0};

	if (name == NULL) {
		LOGE("crash process name is NULL\n");
		return -1;
	}
	sockfd = socket_local_client(SOCKET_NAME, strlen(SOCKET_NAME),
			SOCK_SEQPACKET | SOCK_CLOEXEC);
	if (sockfd == -1) {
		LOGE("failed to connect to usercrashd, error (%s)\n",
			strerror(errno));
		return -1;
	}
	packet.packet_type = kDumpRequest;
	packet.pid = pid;
	strncpy(packet.name, name, COMM_NAME_LEN);
	packet.name[COMM_NAME_LEN - 1] = '\0';
	if (set_timeout(sockfd)) {
		close(sockfd);
		return -1;
	}
	if (write(sockfd, &packet, sizeof(packet)) !=
				sizeof(packet)) {
		LOGE("failed to write DumpRequest packet, error (%s)\n",
			strerror(errno));
		close(sockfd);
		return -1;
	}

	rc = recv_fd(sockfd, &packet, sizeof(packet),
				&tmp_output_fd);
	if (rc == -1) {
		LOGE("failed to read response to DumpRequest packet, ");
		LOGE("error (%s)\n", strerror(errno));
		close(sockfd);
		return -1;
	} else if (rc != sizeof(packet)) {
		LOGE("received DumpRequest response packet of incorrect ");
		LOGE("length (expected %lu, got %ld)\n", sizeof(packet), rc);
		goto fail;
	}
	if (packet.packet_type != kPerformDump) {
		LOGE("unexpected dump response:%d\n", packet.packet_type);
		goto fail;
	}

	/**
	 * Make the fd O_APPEND so that our output is guaranteed to be at the
	 * end of a file. (This also makes selinux rules consistent, because
	 * selinux distinguishes between writing to a regular fd, and writing
	 * to an fd with O_APPEND)
	 */
	flags = fcntl(tmp_output_fd, F_GETFL);
	if (fcntl(tmp_output_fd, F_SETFL, flags | O_APPEND) != 0) {
		LOGE("failed to set output fd flags, error (%s)\n",
					strerror(errno));
		goto fail;
	}

	*usercrashd_socket = sockfd;
	*output_fd = tmp_output_fd;

	return 0;
fail:
	close(sockfd);
	close(tmp_output_fd);
	return -1;

}

/**
 * @usercrashd_socket: client socket fd, used to communicate with server.
 * usercrashd_notify_completion is used to tell the server it has done the
 * dump, the server will pop another crash from the queue and execute the dump
 * process
 */
static int usercrashd_notify_completion(int usercrashd_socket)
{
	struct crash_packet packet = {0};

	packet.packet_type = kCompletedDump;
	if (set_timeout(usercrashd_socket)) {
		close(usercrashd_socket);
		return -1;
	}
	if (write(usercrashd_socket, &packet,
				sizeof(packet)) != sizeof(packet)) {
		return -1;
	}
	return 0;
}

static void print_usage(void)
{
	printf("usercrash - tool to dump crash info for the crashes in the ");
	printf("userspace on sos. It's the client role of usercrash.\n");
	printf("[Usage]\n");
	printf("\t--coredump, usercrash_c <pid> <comm> <sig> ");
	printf("(root role to run)\n");
	printf("[Option]\n");
	printf("\t-h: print this usage message\n");
	printf("\t-v: print usercrash_c version\n");
}

int main(int argc, char *argv[])
{
	int pid;
	int sig;
	int out_fd;
	int sock;
	int ret;

	if (argc > 1) {
		if (strcmp(argv[1], "-v") == 0) {
			printf("version is %d.%d-%s, build by %s@%s\n",
				UC_MAJOR_VERSION, UC_MINOR_VERSION,
				UC_BUILD_VERSION, UC_BUILD_USER,
				UC_BUILD_TIME);
			return 0;
		}
		if (strcmp(argv[1], "-h") == 0) {
			print_usage();
			return 0;
		}
	} else
		print_usage();

	if (getuid() != 0) {
		LOGE("failed to execute usercrash_c, root is required\n");
		exit(EXIT_FAILURE);
	}

	if (argc == 4) {
		/* it's from coredump */
		pid = (int)strtol(argv[1], NULL, 10);
		sig = (int)strtol(argv[3], NULL, 10);
		ret = usercrashd_connect(pid, &sock, &out_fd, argv[2]);
		if (ret) {
			LOGE("usercrashd_connect failed, error (%s)\n",
					strerror(errno));
			exit(EXIT_FAILURE);
		}
		crash_dump(pid, sig, out_fd);
		close(out_fd);
		if (usercrashd_notify_completion(sock)) {
			LOGE("failed to notify usercrashd of completion");
			close(sock);
			exit(EXIT_FAILURE);
		}
		close(sock);
	} else {
		print_usage();
		return 0;
	}

	return 0;
}
