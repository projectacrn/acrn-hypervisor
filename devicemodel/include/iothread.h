/* Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef	_iothread_CTX_H_
#define	_iothread_CTX_H_

#define IOTHREAD_NUM			40

struct iothread_mevent {
	void (*run)(void *);
	void *arg;
	int fd;
};

struct iothread_ctx {
	pthread_t tid;
	int epfd;
	bool started;
	pthread_mutex_t mtx;
	int idx;
};

struct iothreads_info {
	struct iothread_ctx *ioctx_base;
	int num;
};

int iothread_add(struct iothread_ctx *ioctx_x, int fd, struct iothread_mevent *aevt);
int iothread_del(struct iothread_ctx *ioctx_x, int fd);
void iothread_deinit(void);
struct iothread_ctx *iothread_create(int ioctx_num);

#endif
