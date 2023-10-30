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
#include <string.h>

#include "iothread.h"
#include "log.h"
#include "mevent.h"
#include "dm.h"


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

	set_thread_priority(PRIO_IOTHREAD, true);

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
	int ret;

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
	pthread_setname_np(ioctx_x->tid, ioctx_x->name);

	if (CPU_COUNT(&(ioctx_x->cpuset)) != 0) {
		ret = pthread_setaffinity_np(ioctx_x->tid, sizeof(cpuset_t), &(ioctx_x->cpuset));
		if (ret != 0) {
			pr_err("pthread_setaffinity_np fails %d \n", ret);
		}
	}

	pthread_mutex_unlock(&ioctx_x->mtx);
	pr_info("%s started\n", ioctx_x->name);

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
		pr_info("%s stop \n", ioctx_x->name);
	}
	ioctx_active_cnt = 0;
	pthread_mutex_unlock(&ioctxes_mutex);
}

/*
 * Create @ioctx_num iothread context instances
 * Return NULL if fails. Otherwise, return the base of those iothread context instances.
 *
 * Notes:
 * The caller of iothread_create() shall call iothread_free_options() afterwards to free the resources that
 * are dynamically allocated during iothread_parse_options(), such as iothr_opt->cpusets.
 *
 * A general calling sequence from the virtual device owner is like:
 * 1. Call iothread_parse_options() to parse the options from the user.
 * 2. Call iothread_create() to create the iothread instances.
 * 3. Call iothread_free_options() to free the dynamic resources.
 */
struct iothread_ctx *
iothread_create(struct iothreads_option *iothr_opt)
{
	pthread_mutexattr_t attr;
	int i, ret, base, end;
	struct iothread_ctx *ioctx_x;
	struct iothread_ctx *ioctx_base = NULL;
	ret = 0;

	if (iothr_opt == NULL) {
		pr_err("%s: iothr_opt is NULL \n", __func__);
		return ioctx_base;
	}

	pthread_mutex_lock(&ioctxes_mutex);
	base = ioctx_active_cnt;
	end = base + iothr_opt->num;

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

			CPU_ZERO(&(ioctx_x->cpuset));
			if (iothr_opt->cpusets != NULL) {
				memcpy(&(ioctx_x->cpuset), iothr_opt->cpusets + (i - base), sizeof(cpu_set_t));
			}

			if (snprintf(ioctx_x->name, PTHREAD_NAME_MAX_LEN,
				"iothr-%d-%s", ioctx_x->idx, iothr_opt->tag) >= PTHREAD_NAME_MAX_LEN) {
				pr_err("%s: iothread name too long \n", __func__);
			}

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

/*
 * Parse the iothread options from @str and fill the options in @iothr_opt if successes.
 * Return -1 if fails to parse. Otherwise, return 0.
 */
int
iothread_parse_options(char *str, struct iothreads_option *iothr_opt)
{
	char *tmp_num = NULL;
	char *tmp_cpusets = NULL;
	char *tmp_cpux = NULL;
	int service_vm_cpuid, iothread_sub_idx, num;
	cpu_set_t *cpuset_list = NULL;

	/*
	 * Create one iothread instance if DM parameters contain 'iothread', but the number is not specified.
	 */
	num = 1;

	/*
	 * Valid 'iothread' setting examples:
	 * - create 1 iothread instance for virtio-blk
	 *   ... virtio-blk iothread,...
	 *
	 * - create 1 iothread instance for virtio-blk
	 *   ... virtio-blk iothread=1,...
	 *
	 * - create 3 iothread instances for virtio-blk
	 *   ... virtio-blk iothread=3,...
	 *
	 * - create 3 iothread instances for virtio-blk with CPU affinity settings
	 *   ... virtio-blk iothread=3@0:1:2/0:1,...
	 *   CPU affinity of iothread instances for this virtio-blk device:
	 *   - 1st iothread instance <-> Service VM CPU 0,1,2
	 *   - 2nd iothread instance <-> Service VM CPU 0,1
	 *   - 3rd iothread instance <-> No CPU affinity settings
	 *
	 */
	if (str != NULL) {
		/*
		 * "@" is used to separate the following two settings:
		 * - the number of iothread instances
		 * - the CPU affinity settings for each iothread instance.
		 */
		tmp_num = strsep(&str, "@");

		if (tmp_num != NULL) {
			if (dm_strtoi(tmp_num, &tmp_num, 10, &num) || (num <= 0)) {
				pr_err("%s: invalid iothread number %s \n", __func__, tmp_num);
				return -1;
			}

			cpuset_list = calloc(num, sizeof(cpu_set_t));
			if (cpuset_list == NULL) {
				pr_err("%s: calloc cpuset_list returns NULL \n", __func__);
				return -1;
			}

			iothread_sub_idx = 0;
			while ((str != NULL) && (*str !='\0') && (iothread_sub_idx < num)) {
				/* "/" is used to separate the CPU affinity setting for each iothread instance. */
				tmp_cpusets = strsep(&str, "/");

				CPU_ZERO(cpuset_list + iothread_sub_idx);
				while ((tmp_cpusets != NULL) && (*tmp_cpusets !='\0')) {
					/* ":" is used to separate different CPU cores. */
					tmp_cpux = strsep(&tmp_cpusets, ":");

					/*
					 * char '*' can be used to skip the setting for the
					 * specific iothread instance.
					 */
					if (*tmp_cpux == '*') {
						break;
					}

					if (dm_strtoi(tmp_cpux, &tmp_cpux, 10, &service_vm_cpuid) ||
						(service_vm_cpuid < 0)) {
						pr_err("%s: invalid CPU affinity setting %s \n",
							__func__, tmp_cpux);

						free(cpuset_list);
						return -1;
					}

					CPU_SET(service_vm_cpuid, cpuset_list + iothread_sub_idx);
					pr_err("%s: iothread[%d]: set service_vm_cpuid %d \n",
						__func__, iothread_sub_idx, service_vm_cpuid);
				}
				iothread_sub_idx++;
			}
		}
	}
	iothr_opt->num = num;
	iothr_opt->cpusets = cpuset_list;

	return 0;
}

/*
 * This interface is used to free the elements that are allocated dynamically in iothread_parse_options(),
 * such as iothr_opt->cpusets.
 */
void iothread_free_options(struct iothreads_option *iothr_opt)
{
	if ((iothr_opt != NULL) && (iothr_opt->cpusets != NULL)) {
		free(iothr_opt->cpusets);
		iothr_opt->cpusets = NULL;
	}

	return;
}
