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

static struct iothread_ctx ioctxes[IOTHREAD_NUM];
static int ioctx_active_cnt;
/* mutex to protect the free ioctx slot allocation */
static pthread_mutex_t ioctxes_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *
io_thread(void *arg)
{
	struct epoll_event eventlist[MEVENT_MAX];
	struct iothread_mevent *aevp;
	int i, n;
	struct iothread_ctx *ioctx_x = (struct iothread_ctx *)arg;

	while(ioctx_x->started) {
		n = epoll_wait(ioctx_x->epfd, eventlist, MEVENT_MAX, -1);
		if (n < 0) {
			if (errno == EINTR) {
				/* EINTR may happen when io_uring fd is monitored, it is harmless. */
				continue;
			} else {
				pr_err("%s: return from epoll wait with errno %d\r\n", __func__, errno);
				break;
			}
		}
		for (i = 0; i < n; i++) {
			aevp = eventlist[i].data.ptr;
			if (aevp && aevp->run) {
				(*aevp->run)(aevp->arg);
			}
		}
	}

	return NULL;
}

static int
iothread_start(struct iothread_ctx *ioctx_x)
{
	char tname[MAXCOMLEN + 1];
	pthread_mutex_lock(&ioctx_x->mtx);

	if (ioctx_x->started) {
		pthread_mutex_unlock(&ioctx_x->mtx);
		return 0;
	}

	if (pthread_create(&ioctx_x->tid, NULL, io_thread, ioctx_x) != 0) {
		pthread_mutex_unlock(&ioctx_x->mtx);
		pr_err("%s", "iothread create failed\r\n");
		return -1;
	}

	ioctx_x->started = true;
	snprintf(tname, sizeof(tname), "iothread_%d", ioctx_x->idx);
	pthread_setname_np(ioctx_x->tid, tname);
	pthread_mutex_unlock(&ioctx_x->mtx);
	pr_info("iothread_%d started\n", ioctx_x->idx);

	return 0;
}

int
iothread_add(struct iothread_ctx *ioctx_x, int fd, struct iothread_mevent *aevt)
{
	struct epoll_event ee;
	int ret;

	if (ioctx_x == NULL) {
		pr_err("%s: ioctx_x is NULL \n", __func__);
		return -1;
	}

	/* Create a epoll instance before the first fd is added.*/
	ee.events = EPOLLIN;
	ee.data.ptr = aevt;
	ret = epoll_ctl(ioctx_x->epfd, EPOLL_CTL_ADD, fd, &ee);
	if (ret < 0) {
		pr_err("%s: failed to add fd, error is %d\n",
			__func__, errno);
		return ret;
	}

	/* Start the iothread after the first fd is added.*/
	ret = iothread_start(ioctx_x);
	if (ret < 0) {
		pr_err("%s: failed to start iothread thread\n",
			__func__);
	}
	return ret;
}

int
iothread_del(struct iothread_ctx *ioctx_x, int fd)
{
	int ret = 0;

	if (ioctx_x == NULL) {
		pr_err("%s: ioctx_x is NULL \n", __func__);
		return -1;
	}

	if (ioctx_x->epfd) {
		ret = epoll_ctl(ioctx_x->epfd, EPOLL_CTL_DEL, fd, NULL);
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
	int i;
	struct iothread_ctx *ioctx_x;

	pthread_mutex_lock(&ioctxes_mutex);
	for (i = 0; i < ioctx_active_cnt; i++) {
		ioctx_x = &ioctxes[i];

		if (ioctx_x->tid > 0) {
			pthread_mutex_lock(&ioctx_x->mtx);
			ioctx_x->started = false;
			pthread_mutex_unlock(&ioctx_x->mtx);
			pthread_kill(ioctx_x->tid, SIGCONT);
			pthread_join(ioctx_x->tid, &jval);
		}
		if (ioctx_x->epfd > 0) {
			close(ioctx_x->epfd);
			ioctx_x->epfd = -1;
		}
		pthread_mutex_destroy(&ioctx_x->mtx);
		pr_info("iothread_%d stop \n", i);
	}
	ioctx_active_cnt = 0;
	pthread_mutex_unlock(&ioctxes_mutex);
}

/*
 * Create @ioctx_num iothread context instances
 * Return NULL if fails. Otherwise, return the base of those iothread context instances.
 */
struct iothread_ctx *
iothread_create(int ioctx_num)
{
	pthread_mutexattr_t attr;
	int i, ret, base, end;
	struct iothread_ctx *ioctx_x;
	struct iothread_ctx *ioctx_base = NULL;
	ret = 0;

	pthread_mutex_lock(&ioctxes_mutex);
	base = ioctx_active_cnt;
	end = base + ioctx_num;

	if (end > IOTHREAD_NUM) {
		ret = -1;
		pr_err("%s: fails to create new iothread context, max number of instances is %d \n",
			__func__, IOTHREAD_NUM);
	} else {
		for (i = base; i < end; i++) {
			ioctx_x = &ioctxes[i];

			pthread_mutexattr_init(&attr);
			pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
			pthread_mutex_init(&(ioctx_x->mtx), &attr);
			pthread_mutexattr_destroy(&attr);

			ioctx_x->idx = i;
			ioctx_x->tid = 0;
			ioctx_x->started = false;
			ioctx_x->epfd = epoll_create1(0);

			if (ioctx_x->epfd < 0) {
				ret = -1;
				pr_err("%s: failed to create epoll fd, error is %d\r\n",
					__func__, errno);
				break;
			}
		}
		if (ret == 0) {
			ioctx_base = &ioctxes[base];
			ioctx_active_cnt = end;
		}
	}
	pthread_mutex_unlock(&ioctxes_mutex);

	return ioctx_base;
}
