/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <sys/queue.h>
#include <pthread.h>
#include "event_queue.h"
#include "log_sys.h"

const char *etype_str[] = {"CRASH", "INFO", "UPTIME", "HEART_BEAT",
					"REBOOT", "VM", "UNKNOWN"};

static pthread_mutex_t eq_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pcond = PTHREAD_COND_INITIALIZER;
TAILQ_HEAD(, event_t) event_q;

/**
 * Enqueue an event to event_queue.
 *
 * @param event Event to process.
 */
void event_enqueue(struct event_t *event)
{
	pthread_mutex_lock(&eq_mtx);
	TAILQ_INSERT_TAIL(&event_q, event, entries);
	pthread_cond_signal(&pcond);
	LOGD("enqueue %d, (%d)%s\n", event->event_type, event->len,
	     event->path);
	pthread_mutex_unlock(&eq_mtx);
}

/**
 * Count the number of events in event_queue.
 *
 * @return count.
 */
int events_count(void)
{
	struct event_t *e;
	int count = 0;

	pthread_mutex_lock(&eq_mtx);
	TAILQ_FOREACH(e, &event_q, entries)
		count++;
	pthread_mutex_unlock(&eq_mtx);

	return count;
}

/**
 * Dequeue an event from event_queue.
 *
 * @return the dequeued event.
 */
struct event_t *event_dequeue(void)
{
	struct event_t *e;

	pthread_mutex_lock(&eq_mtx);
	while (TAILQ_EMPTY(&event_q))
		pthread_cond_wait(&pcond, &eq_mtx);
	e = TAILQ_FIRST(&event_q);
	TAILQ_REMOVE(&event_q, e, entries);
	LOGD("dequeue %d, (%d)%s\n", e->event_type, e->len, e->path);
	pthread_mutex_unlock(&eq_mtx);

	return e;
}

/**
 * Initailize event_queue.
 */
void init_event_queue(void)
{
	TAILQ_INIT(&event_q);
}
