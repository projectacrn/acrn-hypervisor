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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <malloc.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/thread.h>
#include <signal.h>
#include <stdint.h>
#include <sys/cdefs.h>

#include "packet.h"
#include "protocol.h"
#include "log_sys.h"
#include "version.h"
#include "strutils.h"

#define FILE_PATH_LEN_MAX 256

/**
 * Usercrash works as C/S model: usercrash_s works as usercrash server, which
 * is to handle events from client in endless loop. Once server receives events
 * from client, it will create usercrash_0x file under /var/log/usercrashes/
 * and send file fd to client. Then server will wait for client filling the
 * event info completely to the crash file. After client's work has been done,
 * server will be responsiable to free the crash node and process other events.
 */

struct crash_node {
	int crash_fd;
	int pid;
	int out_fd;
	char name[COMM_NAME_LEN];
	struct event *crash_event;
	char crash_path[FILE_PATH_LEN_MAX];

	TAILQ_ENTRY(crash_node) entries;
};

static const char usercrash_directory[] = "/var/log/usercrashes";
static int usercrash_count = 50;
static int next_usercrash;

static int kMaxConcurrentDumps = 1;
static int num_concurrent_dumps;
TAILQ_HEAD(, crash_node) queue_t;
static pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;


static void push_back(struct crash_node *crash);
static struct crash_node *pop_front(void);

/* Forward declare the callbacks so they can be placed in a sensible order */
static void crash_accept_cb(struct evconnlistener *listener,
		evutil_socket_t sockfd, struct sockaddr*, int, void*);
static void crash_request_cb(evutil_socket_t sockfd, short ev, void *arg);
static void crash_completed_cb(evutil_socket_t sockfd, short ev, void *arg);

static void __attribute__((__unused__)) debuggerd_register_handlers(
			struct sigaction *action)
{
	sigaction(SIGABRT, action, NULL);
	sigaction(SIGBUS, action, NULL);
	sigaction(SIGFPE, action, NULL);
	sigaction(SIGILL, action, NULL);
	sigaction(SIGSEGV, action, NULL);
#if defined(SIGSTKFLT)
	sigaction(SIGSTKFLT, action, NULL);
#endif
	sigaction(SIGSYS, action, NULL);
	sigaction(SIGTRAP, action, NULL);
}

static void free_crash(struct crash_node *crash)
{
	event_free(crash->crash_event);
	close(crash->crash_fd);
	free(crash);
}

static void push_back(struct crash_node *crash)
{
	pthread_mutex_lock(&queue_mtx);
	TAILQ_INSERT_TAIL(&queue_t, crash, entries);
	pthread_mutex_unlock(&queue_mtx);
}

static struct crash_node *pop_front(void)
{
	struct crash_node *crash = NULL;

	pthread_mutex_lock(&queue_mtx);
	if (!TAILQ_EMPTY(&queue_t)) {
		crash = TAILQ_FIRST(&queue_t);
		TAILQ_REMOVE(&queue_t, crash, entries);
	}
	pthread_mutex_unlock(&queue_mtx);

	return crash;
}

static void find_oldest_usercrash(void)
{
	int i;
	int len;
	int oldest_usercrash = 0;
	time_t oldest_time = LONG_MAX;
	char path[FILE_PATH_LEN_MAX];
	struct stat st;

	memset(path, 0, FILE_PATH_LEN_MAX);
	for (i = 0; i < usercrash_count; ++i) {
		len = snprintf(path, sizeof(path), "%s/usercrash_%02d",
				usercrash_directory, i);
		if (s_not_expect(len, sizeof(path))) {
			LOGE("failed to generate path\n");
			continue;
		}
		if (stat(path, &st) != 0) {
			if (errno == ENOENT) {
				oldest_usercrash = i;
				break;
			}

			LOGE("failed to stat %s\n", path);
			continue;
		}

		if (st.st_mtime < oldest_time) {
			oldest_usercrash = i;
			oldest_time = st.st_mtime;
		}
	}
	next_usercrash = oldest_usercrash;
}

static int get_usercrash(struct crash_node *crash)
{
	/**
	 * If kMaxConcurrentDumps is greater than 1, then theoretically the
	 * same filename could be handed out to multiple processes. Unlink and
	 * create the file, instead of using O_TRUNC, to avoid two processes
	 * interleaving their output
	 */
	int result;
	int len;
	char file_name[FILE_PATH_LEN_MAX];

	memset(file_name, 0, FILE_PATH_LEN_MAX);
	len = snprintf(file_name, sizeof(file_name), "%s/usercrash_%02d",
				usercrash_directory, next_usercrash);
	if (s_not_expect(len, sizeof(file_name))) {
		LOGE("failed to generate file name\n");
		return -1;
	}

	if (unlink(file_name) != 0 && errno != ENOENT) {
		LOGE("failed to unlink usercrash at %s\n", file_name);
		return -1;
	}
	result = open(file_name, O_CREAT | O_WRONLY, 0644);
	if (result == -1) {
		LOGE("failed to create usercrash at %s\n", file_name);
		return -1;

	}
	next_usercrash = (next_usercrash + 1) % usercrash_count;
	crash->out_fd = result;
	strncpy(crash->crash_path, file_name, FILE_PATH_LEN_MAX);
	crash->crash_path[FILE_PATH_LEN_MAX - 1] = '\0';
	return 0;
}

static void perform_request(struct crash_node *crash)
{
	ssize_t rc;
	struct crash_packet response = {0};

	if (get_usercrash(crash)) {
		LOGE("server exit for open usercrash file failed\n");
		exit(EXIT_FAILURE);
	}

	LOGI("Prepare to write the '%s' log to %s\n",
				crash->name, crash->crash_path);
	response.packet_type = kPerformDump;
	rc = send_fd(crash->crash_fd, &response,
				sizeof(response), crash->out_fd);
	close(crash->out_fd);
	if (rc == -1) {
		LOGE("failed to send response to CrashRequest\n");
		goto fail;
	} else if (rc != sizeof(response)) {
		LOGE("crash socket write returned short\n");
		goto fail;
	} else {
		struct timeval timeout = { 100, 0 };
		struct event_base *base = event_get_base(crash->crash_event);

		event_assign(crash->crash_event, base, crash->crash_fd,
				EV_TIMEOUT | EV_READ,
				crash_completed_cb, crash);
		event_add(crash->crash_event, &timeout);
	}

	++num_concurrent_dumps;
	return;

fail:
	free_crash(crash);
}

static void dequeue_requests(void)
{
	while (!TAILQ_EMPTY(&queue_t) &&
				num_concurrent_dumps < kMaxConcurrentDumps) {
		struct crash_node *next_crash = pop_front();

		perform_request(next_crash);
	}
}

static void crash_accept_cb(struct evconnlistener *listener,
			evutil_socket_t sockfd,
		struct sockaddr *sa __attribute__((unused)),
		int socklen __attribute__((unused)),
		void *user_data __attribute__((unused)))
{
	struct event_base *base = evconnlistener_get_base(listener);
	struct crash_node *crash = (struct crash_node *)malloc(
					sizeof(struct crash_node));

	struct timeval timeout = { 1, 0 };
	struct event *crash_event = event_new(base, sockfd,
			EV_TIMEOUT | EV_READ, crash_request_cb, crash);

	if (!crash) {
		LOGE("Malloc memory for crash failed.\n");
		exit(EXIT_FAILURE);
	}

	memset(crash, 0, sizeof(struct crash_node));
	crash->crash_fd = sockfd;
	crash->crash_event = crash_event;
	event_add(crash_event, &timeout);
}

static void crash_request_cb(evutil_socket_t sockfd, short ev, void *arg)
{
	ssize_t rc;
	struct crash_node *crash = arg;
	struct crash_packet request = {0};

	if ((ev & EV_TIMEOUT) != 0) {
		LOGE("crash request timed out\n");
		goto fail;
	} else if ((ev & EV_READ) == 0) {
		LOGE("usercrash server received unexpected event ");
		LOGE("from crash socket\n");
		goto fail;
	}

	rc = read(sockfd, &request, sizeof(request));
	if (rc == -1) {
		LOGE("failed to read from crash socket\n");
		goto fail;
	} else if (rc != sizeof(request)) {
		LOGE("crash socket received short read of length %lu ", rc);
		LOGE("(expected %lu)\n", sizeof(request));
		goto fail;
	}

	if (request.packet_type != kDumpRequest) {
		LOGE("unexpected crash packet type, expected kDumpRequest, ");
		LOGE("received %d\n", request.packet_type);
		goto fail;
	}
	crash->pid = request.pid;
	strncpy(crash->name, request.name, COMM_NAME_LEN);
	crash->name[COMM_NAME_LEN - 1] = '\0';
	LOGI("received crash request from pid %d, name: %s\n",
				crash->pid, crash->name);

	if (num_concurrent_dumps == kMaxConcurrentDumps) {
		LOGI("enqueueing crash request for pid %d\n", crash->pid);
		push_back(crash);
	} else {
		perform_request(crash);
	}

	return;

fail:
	free_crash(crash);
}

static void crash_completed_cb(evutil_socket_t sockfd, short ev, void *arg)
{
	ssize_t rc;
	struct crash_node *crash = arg;
	struct crash_packet request = {0};

	--num_concurrent_dumps;

	if ((ev & EV_TIMEOUT) != 0) {
		LOGE("error for crash request timed out, error (%s)\n",
			strerror(errno));
		goto out;
	} else if ((ev & EV_READ) == 0) {
		LOGE("usercrash server received unexpected event ");
		LOGE("from crash socket\n");
		goto out;
	}

	rc = read(sockfd, &request, sizeof(request));
	if (rc == -1) {
		LOGE("failed to read from crash socket\n");
		goto out;
	} else if (rc != sizeof(request)) {
		LOGE("crash socket received short read of length %lu, ", rc);
		LOGE("(expected %lu)\n", sizeof(request));
		goto out;
	}

	if (request.packet_type != kCompletedDump) {
		LOGE("unexpected crash packet type, expected kCompletedDump, ");
		LOGE("received %d\n", request.packet_type);
		goto out;
	}

	if (crash->crash_path) {
		LOGI("usercrash log written to: %s, ", crash->crash_path);
		LOGI("crash process name is: %s\n", crash->name);
	}

out:
	free_crash(crash);
	/* If there's something queued up, let them proceed */
	dequeue_requests();
}

static void sig_handler(int sig)
{
	LOGE("received fatal signal: %d\n", sig);
	exit(EXIT_FAILURE);
}

static void print_usage(void)
{
	printf("usercrash - tool to dump crash info for the crashes in the ");
	printf("userspace on sos. It's the server role of usercrash.\n");
	printf("[Usage] usercrash_s (root role to run)\n");
	printf("[Option]\n");
	printf("\t-h: print this usage message\n");
	printf("\t-v: print usercrash_s version\n");
	printf("[Output] crash log is in %s folder\n", usercrash_directory);
}

int main(int argc, char *argv[])
{
	char socket_path[SOCKET_PATH_MAX];
	DIR *dir;
	int fd;
	int len;
	evutil_socket_t crash_socket;
	int opt;
	struct sigaction action;
	struct event_base *base;
	struct evconnlistener *listener;

	if (argc > 1) {
		while ((opt = getopt(argc, argv, "vh")) != -1) {
			switch (opt) {
			case 'v':
				printf("version is %d.%d-%s, build by %s@%s\n",
					UC_MAJOR_VERSION, UC_MINOR_VERSION,
					UC_BUILD_VERSION, UC_BUILD_USER,
					UC_BUILD_TIME);
				break;
			case 'h':
				print_usage();
				break;
			default:
				printf("unknown option\n");
				exit(EXIT_FAILURE);
			}
		}
		return 0;
	}

	if (getuid() != 0) {
		LOGE("failed to boot usercrash_s, root is required\n");
		exit(EXIT_FAILURE);
	}

	dir = opendir(usercrash_directory);
	if (dir == NULL) {
		fd = mkdir(usercrash_directory, 0755);
		if (fd == -1) {
			LOGE("create log folder failed, error (%s)\n",
					strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (chmod(usercrash_directory, 0755) == -1) {
			LOGE("failed to change usercrash folder priority, ");
			LOGE("error (%s)\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else {
		closedir(dir);
	}

	/* Don't try to connect to ourselves if we crash */
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = sig_handler;
	debuggerd_register_handlers(&action);

	find_oldest_usercrash();
	len = snprintf(socket_path, sizeof(socket_path), "%s/%s",
			RESERVED_SOCKET_PREFIX, SOCKET_NAME);
	if (s_not_expect(len , sizeof(socket_path))) {
		LOGE("construct socket path error\n");
		exit(EXIT_FAILURE);
	}
	/**
	 * evutil_socket_t on other platform except WIN32 platform is int
	 * type, but on WIN32 platform evutil_socket_t is intptr_t type.
	 * So, considering compatibility, here need to transfer socket fd to
	 * evutil_socket_t type.
	 */
	crash_socket = (evutil_socket_t)create_socket_server(socket_path,
			SOCK_SEQPACKET);

	if (crash_socket == -1) {
		LOGE("failed to get socket from init, error (%s)\n",
					strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (chmod(socket_path, 0622) == -1) {
		LOGE("failed to change usercrashd_crash priority\n");
		goto fail;
	}

	evutil_make_socket_nonblocking(crash_socket);

	base = event_base_new();
	if (!base) {
		LOGE("failed to create event_base\n");
		goto fail;
	}

	TAILQ_INIT(&queue_t);
	listener = evconnlistener_new(base, crash_accept_cb, NULL,
			LEV_OPT_CLOSE_ON_FREE, -1, crash_socket);
	if (!listener) {
		LOGE("failed to create evconnlistener: %s\n", strerror(errno));
		goto fail;
	}

	LOGI("usercrash_s successfully initialized\n");
	event_base_dispatch(base);

	evconnlistener_free(listener);
	event_base_free(base);

	close(crash_socket);
	return 0;

fail:
	close(crash_socket);
	exit(EXIT_FAILURE);
}
