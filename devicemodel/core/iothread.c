/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/queue.h>
#include <pthread.h>
#include <signal.h>

#include "iothread.h"
#include "log.h"
#include "mevent.h"


#define MEVENT_MAX 64
#define MAX_EVENT_NUM 64
struct iothread_ctx {
	pthread_t tid;
	int epfd;
	bool started;
	pthread_mutex_t mtx;
};
static struct iothread_ctx ioctx;

static void *
io_thread(void *arg)
{
	struct epoll_event eventlist[MEVENT_MAX];
	struct iothread_mevent *aevp;
	int i, n, status;
	char buf[MAX_EVENT_NUM];

	while(ioctx.started) {
		n = epoll_wait(ioctx.epfd, eventlist, MEVENT_MAX, -1);
		if (n < 0) {
			if (errno == EINTR)
				pr_info("%s: exit from epoll_wait\n", __func__);
			else
				pr_err("%s: return from epoll wait with errno %d\r\n", __func__, errno);
			break;
		}
		for (i = 0; i < n; i++) {
			aevp = eventlist[i].data.ptr;
			if (aevp && aevp->run) {
				/* Mitigate the epoll_wait repeat cycles by reading out the events as more as possile.*/
				do {
					status = read(aevp->fd, buf, sizeof(buf));
				} while (status == MAX_EVENT_NUM);
				(*aevp->run)(aevp->arg);
			}
		}
	}

	return NULL;
}

static int
iothread_start(void)
{
	pthread_mutex_lock(&ioctx.mtx);

	if (ioctx.started) {
		pthread_mutex_unlock(&ioctx.mtx);
		return 0;
	}

	if (pthread_create(&ioctx.tid, NULL, io_thread, NULL) != 0) {
		pthread_mutex_unlock(&ioctx.mtx);
		pr_err("%s", "iothread create failed\r\n");
		return -1;
	}
	ioctx.started = true;
	pthread_setname_np(ioctx.tid, "iothread");
	pthread_mutex_unlock(&ioctx.mtx);
	pr_info("iothread started\n");
	return 0;
}

int
iothread_add(int fd, struct iothread_mevent *aevt)
{
	struct epoll_event ee;
	int ret;
	/* Create a epoll instance before the first fd is added.*/
	ee.events = EPOLLIN;
	ee.data.ptr = aevt;
	ret = epoll_ctl(ioctx.epfd, EPOLL_CTL_ADD, fd, &ee);
	if (ret < 0) {
		pr_err("%s: failed to add fd, error is %d\n",
			__func__, errno);
		return ret;
	}

	/* Start the iothread after the first fd is added.*/
	ret = iothread_start();
	if (ret < 0) {
		pr_err("%s: failed to start iothread thread\n",
			__func__);
	}
	return ret;
}

int
iothread_del(int fd)
{
	int ret = 0;

	if (ioctx.epfd) {
		ret = epoll_ctl(ioctx.epfd, EPOLL_CTL_DEL, fd, NULL);
		if (ret < 0)
			pr_err("%s: failed to delete fd from epoll fd, error is %d\n",
				__func__, errno);
	}
	return ret;
}

void
iothread_deinit(void)
{
	void *jval;

	if (ioctx.tid > 0) {
		pthread_mutex_lock(&ioctx.mtx);
		ioctx.started = false;
		pthread_mutex_unlock(&ioctx.mtx);
		pthread_kill(ioctx.tid, SIGCONT);
		pthread_join(ioctx.tid, &jval);
	}
	if (ioctx.epfd > 0) {
		close(ioctx.epfd);
		ioctx.epfd = -1;
	}
	pthread_mutex_destroy(&ioctx.mtx);
	pr_info("iothread stop\n");
}

int
iothread_init(void)
{
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&ioctx.mtx, &attr);
	pthread_mutexattr_destroy(&attr);

	ioctx.tid = 0;
	ioctx.started = false;
	ioctx.epfd = epoll_create1(0);

	if (ioctx.epfd < 0) {
		pr_err("%s: failed to create epoll fd, error is %d\r\n",
			__func__, errno);
		return -1;
	}
	return 0;
}
