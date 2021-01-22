/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __EVENT_QUEUE_H__
#define __EVENT_QUEUE_H__

#include <sys/queue.h>

enum event_type_t {
	CRASH,
	INFO,
	UPTIME,
	HEART_BEAT,
	REBOOT,
	VM,
	UNKNOWN
};

extern const char *etype_str[];

__extension__
struct event_t {
	int watchfd;
	enum event_type_t event_type;
	const char *channel;
	void *private;

	TAILQ_ENTRY(event_t) entries;

	/* dir to storage logs */
	char *dir;
	size_t dlen;
	int len;
	char path[0]; /* keep this at tail*/
};

void event_enqueue(struct event_t *event);
int events_count(void);
struct event_t *event_dequeue(void);
void init_event_queue(void);

#endif
