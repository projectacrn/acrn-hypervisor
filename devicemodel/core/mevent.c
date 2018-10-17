/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Micro event library for FreeBSD, designed for a single i/o thread
 * using EPOLL, and having events be persistent by default.
 */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/queue.h>
#include <pthread.h>

#include "mevent.h"
#include "vmmapi.h"

#define	MEVENT_MAX	64

#define	MEV_ADD		1
#define	MEV_ENABLE	2
#define	MEV_DISABLE	3
#define	MEV_DEL_PENDING	4

static int epoll_fd;
static pthread_t mevent_tid;
static int mevent_pipefd[2];
static pthread_mutex_t mevent_lmutex = PTHREAD_MUTEX_INITIALIZER;

struct mevent {
	void			(*me_func)(int, enum ev_type, void *);
	int			me_fd;
	enum			ev_type me_type;
	void			*me_param;
	int			me_cq;
	int			me_state;

	int			closefd;
	LIST_ENTRY(mevent)	me_list;
};

static LIST_HEAD(listhead, mevent) global_head;
/* List holds the mevent node which is requested to deleted */
static LIST_HEAD(del_listhead, mevent) del_head;

static void
mevent_qlock(void)
{
	pthread_mutex_lock(&mevent_lmutex);
}

static void
mevent_qunlock(void)
{
	pthread_mutex_unlock(&mevent_lmutex);
}

static bool
is_dispatch_thread(void)
{
	return (pthread_self() == mevent_tid);
}

static void
mevent_pipe_read(int fd, enum ev_type type, void *param)
{
	char buf[MEVENT_MAX];
	int status;

	/*
	 * Drain the pipe read side. The fd is non-blocking so this is
	 * safe to do.
	 */
	do {
		status = read(fd, buf, sizeof(buf));
	} while (status == MEVENT_MAX);
}

/*On error, -1 is returned, else return zero*/
int
mevent_notify(void)
{
	char c;

	/*
	 * If calling from outside the i/o thread, write a byte on the
	 * pipe to force the i/o thread to exit the blocking epoll call.
	 */
	if (mevent_pipefd[1] != 0 && pthread_self() != mevent_tid)
		if (write(mevent_pipefd[1], &c, 1) <= 0)
			return -1;
	return 0;
}

static int
mevent_kq_filter(struct mevent *mevp)
{
	int retval;

	retval = 0;

	if (mevp->me_type == EVF_READ)
		retval = EPOLLIN;

	if (mevp->me_type == EVF_READ_ET)
		retval = EPOLLIN | EPOLLET;

	if (mevp->me_type == EVF_WRITE)
		retval = EPOLLOUT;

	if (mevp->me_type == EVF_WRITE_ET)
		retval = EPOLLOUT | EPOLLET;

	return retval;
}

static void
mevent_destroy(void)
{
	struct mevent *mevp, *tmpp;

	mevent_qlock();
	list_foreach_safe(mevp, &global_head, me_list, tmpp) {
		LIST_REMOVE(mevp, me_list);
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, mevp->me_fd, NULL);

		if ((mevp->me_type == EVF_READ ||
		     mevp->me_type == EVF_READ_ET ||
		     mevp->me_type == EVF_WRITE ||
		     mevp->me_type == EVF_WRITE_ET) &&
		     mevp->me_fd != STDIN_FILENO)
			close(mevp->me_fd);

		free(mevp);
	}

	/* the mevp in del_head was removed from epoll when add it
	 * to del_head already.
	 */
	list_foreach_safe(mevp, &del_head, me_list, tmpp) {
		LIST_REMOVE(mevp, me_list);

		if ((mevp->me_type == EVF_READ ||
		     mevp->me_type == EVF_READ_ET ||
		     mevp->me_type == EVF_WRITE ||
		     mevp->me_type == EVF_WRITE_ET) &&
		     mevp->me_fd != STDIN_FILENO)
			close(mevp->me_fd);

		free(mevp);
	}
	mevent_qunlock();
}

static void
mevent_handle(struct epoll_event *kev, int numev)
{
	int i;
	struct mevent *mevp;

	for (i = 0; i < numev; i++) {
		mevp = kev[i].data.ptr;

		if (mevp->me_state)
			(*mevp->me_func)(mevp->me_fd, mevp->me_type, mevp->me_param);
	}
}

struct mevent *
mevent_add(int tfd, enum ev_type type,
	   void (*func)(int, enum ev_type, void *), void *param)
{
	int ret;
	struct epoll_event ee;
	struct mevent *lp, *mevp;

	if (tfd < 0 || func == NULL)
		return NULL;

	if (type == EVF_TIMER)
		return NULL;

	mevent_qlock();
	/* Verify that the fd/type tuple is not present in the list */
	LIST_FOREACH(lp, &global_head, me_list) {
		if (lp->me_fd == tfd && lp->me_type == type) {
			mevent_qunlock();
			return lp;
		}
	}
	mevent_qunlock();

	/*
	 * Allocate an entry, populate it, and add it to the list.
	 */
	mevp = calloc(1, sizeof(struct mevent));
	if (mevp == NULL)
		return NULL;

	mevp->me_fd = tfd;
	mevp->me_type = type;
	mevp->me_func = func;
	mevp->me_param = param;
	mevp->me_state = 1;

	ee.events = mevent_kq_filter(mevp);
	ee.data.ptr = mevp;
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, mevp->me_fd, &ee);
	if (ret == 0) {
		mevent_qlock();
		LIST_INSERT_HEAD(&global_head, mevp, me_list);
		mevent_qunlock();

		return mevp;
	} else {
		free(mevp);
		return NULL;
	}
}

int
mevent_enable(struct mevent *evp)
{
	int ret;
	struct epoll_event ee;
	struct mevent *lp, *mevp = NULL;

	mevent_qlock();
	/* Verify that the fd/type tuple is not present in the list */
	LIST_FOREACH(lp, &global_head, me_list) {
		if (lp == evp) {
			mevp = lp;
			break;
		}
	}
	mevent_qunlock();

	if (!mevp)
		return -1;

	ee.events = mevent_kq_filter(mevp);
	ee.data.ptr = mevp;
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, mevp->me_fd, &ee);
	if (ret < 0 && errno == EEXIST)
		ret = 0;

	return ret;
}

int
mevent_disable(struct mevent *evp)
{
	int ret;

	ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, evp->me_fd, NULL);
	if (ret < 0 && errno == ENOENT)
		ret = 0;

	return ret;
}

static void
mevent_add_to_del_list(struct mevent *evp, int closefd)
{
	mevent_qlock();
	LIST_INSERT_HEAD(&del_head, evp, me_list);
	mevent_qunlock();

	mevent_notify();
}

static void
mevent_drain_del_list(void)
{
	struct mevent *evp, *tmpp;

	mevent_qlock();
	list_foreach_safe(evp, &del_head, me_list, tmpp) {
		LIST_REMOVE(evp, me_list);
		if (evp->closefd) {
			close(evp->me_fd);
		}
		free(evp);
	}
	mevent_qunlock();
}

static int
mevent_delete_event(struct mevent *evp, int closefd)
{
	mevent_qlock();
	LIST_REMOVE(evp, me_list);
	mevent_qunlock();
	evp->me_state = 0;
	evp->closefd = closefd;

	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, evp->me_fd, NULL);
	if (!is_dispatch_thread()) {
		mevent_add_to_del_list(evp, closefd);
	} else {
		if (evp->closefd) {
			close(evp->me_fd);
		}
		free(evp);
	}
	return 0;
}

int
mevent_delete(struct mevent *evp)
{
	return mevent_delete_event(evp, 0);
}

int
mevent_delete_close(struct mevent *evp)
{
	return mevent_delete_event(evp, 1);
}

static void
mevent_set_name(void)
{
	pthread_setname_np(mevent_tid, "mevent");
}

int
mevent_init(void)
{
	epoll_fd = epoll_create1(0);
	assert(epoll_fd >= 0);

	if (epoll_fd >= 0)
		return 0;
	else
		return -1;
}

void
mevent_deinit(void)
{
	mevent_destroy();
	close(epoll_fd);
	if (mevent_pipefd[1] != 0)
		close(mevent_pipefd[1]);
}

void
mevent_dispatch(void)
{
	struct epoll_event eventlist[MEVENT_MAX];

	struct mevent *pipev;
	int ret;

	mevent_tid = pthread_self();
	mevent_set_name();

	/*
	 * Open the pipe that will be used for other threads to force
	 * the blocking kqueue call to exit by writing to it. Set the
	 * descriptor to non-blocking.
	 */
	ret = pipe(mevent_pipefd);
	if (ret < 0) {
		perror("pipe");
		exit(0);
	}

	/*
	 * Add internal event handler for the pipe write fd
	 */
	pipev = mevent_add(mevent_pipefd[0], EVF_READ, mevent_pipe_read, NULL);
	assert(pipev != NULL);

	for (;;) {
		int suspend_mode;

		/*
		 * Block awaiting events
		 */
		ret = epoll_wait(epoll_fd, eventlist, MEVENT_MAX, -1);
		if (ret == -1 && errno != EINTR)
			perror("Error return from epoll_wait");

		/*
		 * Handle reported events
		 */
		mevent_handle(eventlist, ret);
		mevent_drain_del_list();

		suspend_mode = vm_get_suspend_mode();
		if ((suspend_mode != VM_SUSPEND_NONE) &&
		    (suspend_mode != VM_SUSPEND_SYSTEM_RESET) &&
		    (suspend_mode != VM_SUSPEND_SUSPEND))
			break;
	}
}
