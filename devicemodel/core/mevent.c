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

#include <sys/cdefs.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/queue.h>
#include <pthread.h>

#include "mevent.h"
#include "vmm.h"
#include "vmmapi.h"

#define	MEVENT_MAX	64

#define	MEV_ADD		1
#define	MEV_ENABLE	2
#define	MEV_DISABLE	3
#define	MEV_DEL_PENDING	4

static pthread_t mevent_tid;
static int mevent_pipefd[2];
static pthread_mutex_t mevent_lmutex = PTHREAD_MUTEX_INITIALIZER;

struct mevent {
	void	(*me_func)(int, enum ev_type, void *);
	int	me_fd;
	enum ev_type me_type;
	void *me_param;
	int	me_cq;
	int	me_state;
	int	me_closefd;

	LIST_ENTRY(mevent) me_list;
};

struct ctl_event {
	int op;
	int fd;
	struct epoll_event ee;
};

static LIST_HEAD(listhead, mevent) global_head, change_head;

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

	if (mevp->me_type == EVF_WRITE)
		retval = EPOLLOUT;
	return retval;
}

static int
mevent_kq_flags(struct mevent *mevp)
{
	int ret;

	switch (mevp->me_state) {
	case MEV_ADD:
		ret = EPOLL_CTL_ADD;		/* implicitly enabled */
		break;
	case MEV_DEL_PENDING:
		ret = EPOLL_CTL_DEL;
		break;
	default:
		assert(0);
		break;
	}

	return ret;
}

static int
mevent_build(int mfd, struct ctl_event *kev)
{
	struct mevent *mevp, *tmpp;
	int i;

	i = 0;

	mevent_qlock();

	list_foreach_safe(mevp, &change_head, me_list, tmpp) {
		if (mevp->me_closefd) {
			/*
			 * A close of the file descriptor will remove the
			 * event
			 */
			close(mevp->me_fd);
		} else {
			kev[i].fd = mevp->me_fd;
			kev[i].ee.events = mevent_kq_filter(mevp);
			kev[i].op = mevent_kq_flags(mevp);
			kev[i].ee.data.ptr = mevp;
			i++;
		}

		mevp->me_cq = 0;
		LIST_REMOVE(mevp, me_list);

		if (mevp->me_state == MEV_DEL_PENDING)
			free(mevp);
		else
			LIST_INSERT_HEAD(&global_head, mevp, me_list);

		assert(i < MEVENT_MAX);
	}

	mevent_qunlock();

	return i;
}

static void
mevent_destroy()
{
	struct mevent *mevp, *tmpp;

	mevent_qlock();

	list_foreach_safe(mevp, &global_head, me_list, tmpp) {
		if ((mevp->me_type == EVF_READ ||
			mevp->me_type == EVF_WRITE)
			&& mevp->me_fd != STDIN_FILENO)
			close(mevp->me_fd);
		LIST_REMOVE(mevp, me_list);
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
		/* XXX check for EV_ERROR ? */

		(*mevp->me_func)(mevp->me_fd, mevp->me_type, mevp->me_param);
	}
}

struct mevent *
mevent_add(int tfd, enum ev_type type,
	   void (*func)(int, enum ev_type, void *), void *param)
{
	struct mevent *lp, *mevp;

	if (tfd < 0 || func == NULL)
		return NULL;

	if (type == EVF_TIMER)
		return NULL;

	mevp = NULL;

	mevent_qlock();

	/*
	 * Verify that the fd/type tuple is not present in any list
	 */
	LIST_FOREACH(lp, &global_head, me_list) {
		if (lp->me_fd == tfd && lp->me_type == type)
			goto exit;
	}

	LIST_FOREACH(lp, &change_head, me_list) {
		if (lp->me_fd == tfd && lp->me_type == type)
			goto exit;
	}

	/*
	 * Allocate an entry, populate it, and add it to the change list.
	 */
	mevp = calloc(1, sizeof(struct mevent));
	if (mevp == NULL)
		goto exit;

	mevp->me_fd = tfd;
	mevp->me_type = type;
	mevp->me_func = func;
	mevp->me_param = param;

	LIST_INSERT_HEAD(&change_head, mevp, me_list);
	mevp->me_cq = 1;
	mevp->me_state = MEV_ADD;
	mevent_notify();

exit:
	mevent_qunlock();

	return mevp;
}

int
mevent_enable(struct mevent *evp)
{
	return 0;
}

int
mevent_disable(struct mevent *evp)
{
	return 0;
}

static int
mevent_delete_event(struct mevent *evp, int closefd)
{
	mevent_qlock();

	/*
	 * Place the entry onto the changed list if not already there, and
	 * mark as to be deleted.
	 */
	if (evp->me_cq == 0) {
		evp->me_cq = 1;
		LIST_REMOVE(evp, me_list);
		LIST_INSERT_HEAD(&change_head, evp, me_list);
		mevent_notify();
	}
	evp->me_state = MEV_DEL_PENDING;

	if (closefd)
		evp->me_closefd = 1;

	mevent_qunlock();

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

void
mevent_dispatch(void)
{
	struct ctl_event clist[MEVENT_MAX];
	struct epoll_event eventlist[MEVENT_MAX];

	struct mevent *pipev;
	int mfd;
	int numev;
	int ret;

	mevent_tid = pthread_self();
	mevent_set_name();

	mfd = epoll_create1(0);
	assert(mfd > 0);

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
		/*
		 * Build changelist if required.
		 * XXX the changelist can be put into the blocking call
		 * to eliminate the extra syscall. Currently better for
		 * debug.
		 */
		int i;
		struct epoll_event *e;

		numev = mevent_build(mfd, clist);

		for (i = 0; i < numev; i++) {
			e = &clist[i].ee;
			ret = epoll_ctl(mfd, clist[i].op, clist[i].fd, e);
			if (ret == -1)
				perror("Error return from epoll_ctl");
		}

		/*
		 * Block awaiting events
		 */
		ret = epoll_wait(mfd, eventlist, MEVENT_MAX, -1);
		if (ret == -1 && errno != EINTR)
			perror("Error return from epoll_wait");

		/*
		 * Handle reported events
		 */
		mevent_handle(eventlist, ret);

		if (vm_get_suspend_mode() != VM_SUSPEND_NONE)
			break;
	}
	mevent_build(mfd, clist);
	mevent_destroy();
	close(mfd);
}
