/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _CHANNELS_H
#define _CHANNELS_H

#define BASE_DIR_MASK		(IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF)
#define UPTIME_MASK		IN_CLOSE_WRITE
#define MAXEVENTS 15
#define HEART_RATE (6 * 1000) /* ms */

struct channel_t {
	char *name;
	int fd;
	void (*channel_fn)(struct channel_t *);
};
extern int create_detached_thread(pthread_t *pid,
				void *(*fn)(void *), void *arg);
extern int init_channels(void);


#endif
