/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>
#include <stdlib.h>
#include "event_queue.h"
#include "load_conf.h"
#include "channels.h"
#include "fsutils.h"
#include "cmdutils.h"
#include "log_sys.h"
#include "event_handler.h"
#include "startupreason.h"

/* Watchdog timeout in second*/
#define WDT_TIMEOUT 300

static struct event_t *last_e;
static int event_processing;

/**
 * Handle watchdog expire.
 *
 * @param signal Signal which triggered this function.
 */
static void wdt_timeout(int signal)
{
	struct event_t *e;
	struct crash_t *crash;
	struct info_t *info;
	int count;

	if (signal == SIGALRM) {
		LOGE("haven't received heart beat(%ds) for %ds, killing self\n",
		     HEART_BEAT, WDT_TIMEOUT);

		if (event_processing) {
			LOGE("event (%d, %s) processing...\n",
			     last_e->event_type, last_e->path);
			free(last_e);
		}

		count = events_count();
		LOGE("total %d unhandled events :\n", count);

		while (count-- && (e = event_dequeue())) {
			switch (e->event_type) {
			case CRASH:
				crash = (struct crash_t *)e->private;
				LOGE("CRASH (%s, %s)\n", (char *)crash->name,
				     e->path);
				break;
			case INFO:
				info = (struct info_t *)e->private;
				LOGE("INFO (%s)\n", (char *)info->name);
				break;
			case UPTIME:
				LOGE("UPTIME\n");
				break;
			case HEART_BEAT:
				LOGE("HEART_BEAT\n");
				break;
			case REBOOT:
				LOGE("REBOOT\n");
				break;
			default:
				LOGE("error event type %d\n", e->event_type);
			}
			free(e);
		}

		raise(SIGKILL);
	}
}

/**
 * Fed watchdog.
 *
 * @param timeout in second When the watchdog expire next time.
 */
static void watchdog_fed(int timeout)
{
	struct itimerval new_value;
	int ret;

	memset(&new_value, 0, sizeof(new_value));

	new_value.it_value.tv_sec = timeout;
	ret = setitimer(ITIMER_REAL, &new_value, NULL);
	if (ret < 0) {
		LOGE("setitimer failed, error (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/**
 * Initialize watchdog. This watchdog is used to monitor event handler.
 *
 * @param timeout in second When the watchdog expire next time.
 */
static void watchdog_init(int timeout)
{
	struct itimerval new_value;
	int ret;
	sighandler_t ohdlr;

	ohdlr = signal(SIGALRM, wdt_timeout);
	if (ohdlr == SIG_ERR) {
		LOGE("signal failed, error (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	memset(&new_value, 0, sizeof(new_value));

	new_value.it_value.tv_sec = timeout;
	ret = setitimer(ITIMER_REAL, &new_value, NULL);
	if (ret < 0) {
		LOGE("setitimer failed, error (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/**
 * Process each event in event queue.
 * Note that currently event handler is single threaded.
 */
static void *event_handle(void *unused __attribute__((unused)))
{
	int id;
	struct sender_t *sender;
	struct event_t *e;

	while ((e = event_dequeue())) {
		/* here we only handle internal event */
		if (e->event_type == HEART_BEAT) {
			watchdog_fed(WDT_TIMEOUT);
			free(e);
			continue;
		}

		/* last_e is allocated for debug purpose, the information
		 * will be dumped if watchdog expire.
		 */
		last_e = malloc(sizeof(*e) + e->len);
		if (last_e == NULL) {
			LOGE("malloc failed, error (%s)\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		event_processing = 1;
		memcpy(last_e, e, sizeof(*e) + e->len);

		for_each_sender(id, sender, conf) {
			if (!sender)
				continue;

			if (sender->send)
				sender->send(e);
		}

		if (e->event_type == REBOOT) {
			char reason[REBOOT_REASON_SIZE];

			read_startupreason(reason, sizeof(reason));
			if (!strcmp(reason, "WARM") ||
			    !strcmp(reason, "WATCHDOG"))
				if (exec_out2file(NULL, "reboot") == -1)
					break;
		}

		if ((e->dir))
			free(e->dir);
		free(e);
		event_processing = 0;
		free(last_e);

	}

	LOGE("failed to reboot system, %s exit\n", __func__);
	return NULL;
}

/**
 * Initialize event handler.
 */
int init_event_handler(void)
{
	int ret;
	pthread_t pid;

	watchdog_init(WDT_TIMEOUT);
	ret = create_detached_thread(&pid, &event_handle, NULL);
	if (ret) {
		LOGE("create event handler failed (%s)\n", strerror(errno));
		return -1;
	}
	return 0;
}
