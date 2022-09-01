/* Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef	_iothread_CTX_H_
#define	_iothread_CTX_H_

struct iothread_mevent {
	void (*run)(void *);
	void *arg;
	int fd;
};
int iothread_add(int fd, struct iothread_mevent *aevt);
int iothread_del(int fd);
int iothread_init(void);
void iothread_deinit(void);

#endif
