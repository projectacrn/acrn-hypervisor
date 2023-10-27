/* Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef	_iothread_CTX_H_
#define	_iothread_CTX_H_

#define IOTHREAD_NUM			40

/*
 * The pthread_setname_np() function can be used to set a unique name for a thread,
 * which can be useful for debugging multithreaded applications.
 * The thread name is a meaningful C language string,
 * whose length is restricted to 16 characters, including the terminating null byte ('\0').
 */
#define PTHREAD_NAME_MAX_LEN		16

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
	char name[PTHREAD_NAME_MAX_LEN];
};

struct iothreads_info {
	struct iothread_ctx *ioctx_base;
	int num;
};

int iothread_add(struct iothread_ctx *ioctx_x, int fd, struct iothread_mevent *aevt);
int iothread_del(struct iothread_ctx *ioctx_x, int fd);
void iothread_deinit(void);
struct iothread_ctx *iothread_create(int ioctx_num, const char *ioctx_tag);

#endif
