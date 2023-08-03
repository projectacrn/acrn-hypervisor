/*
 * Copyright (C) 2018-2023 Intel Corporation.
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
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <acrn_common.h>

#include "vm_event.h"
#include "hsm_ioctl_defs.h"
#include "sbuf.h"
#include "log.h"
#include <cjson/cJSON.h>
#include "monitor.h"
#include "timer.h"

#define VM_EVENT_ELE_SIZE (sizeof(struct vm_event))

#define HV_VM_EVENT_TUNNEL 0
#define DM_VM_EVENT_TUNNEL 1
#define MAX_VM_EVENT_TUNNELS 2
#define MAX_EPOLL_EVENTS MAX_VM_EVENT_TUNNELS

#define THROTTLE_WINDOW	1U /* time window for throttle counter, in secs*/

typedef void (*vm_event_handler)(struct vmctx *ctx, struct vm_event *event);
typedef void (*vm_event_generate_jdata)(cJSON *event_obj, struct vm_event *event);

static int epoll_fd;
static bool started = false;
static char hv_vm_event_page[4096] __aligned(4096);
static char dm_vm_event_page[4096] __aligned(4096);
static pthread_t vm_event_tid;

static void general_event_handler(struct vmctx *ctx, struct vm_event *event);

enum event_source_type {
	EVENT_SOURCE_TYPE_HV,
	EVENT_SOURCE_TYPE_DM,
};

struct vm_event_tunnel {
	enum event_source_type type;
	struct shared_buf *sbuf;
	uint32_t sbuf_size;
	int kick_fd;
	pthread_mutex_t mtx;
	bool enabled;
};

struct event_throttle_ctl {
	struct acrn_timer timer;
	pthread_mutex_t mtx;
	uint32_t event_counter;
	uint32_t throttle_count;	/* how many events has been throttled(dropped) */
	bool	is_up;
};

struct vm_event_proc {
	vm_event_handler ve_handler;
	uint32_t	throttle_rate; /* how many events allowed per sec */
	struct event_throttle_ctl throttle_ctl;
	vm_event_generate_jdata gen_jdata_handler;
};


static struct vm_event_proc ve_proc[VM_EVENT_COUNT] = {
	[VM_EVENT_RTC_CHG] = {
		.ve_handler = general_event_handler,
		.gen_jdata_handler = NULL,
		.throttle_rate = 1,
	},
	[VM_EVENT_POWEROFF] = {
		.ve_handler = general_event_handler,
		.gen_jdata_handler = NULL,
		.throttle_rate = 1,
	},
	[VM_EVENT_TRIPLE_FAULT] = {
		.ve_handler = general_event_handler,
		.gen_jdata_handler = NULL,
		.throttle_rate = 1,
	},
};

static inline struct vm_event_proc *get_vm_event_proc(struct vm_event *event)
{
	struct vm_event_proc *proc = NULL;
	if (event->type < VM_EVENT_COUNT) {
		proc = &ve_proc[event->type];
	}
	return proc;
}

static bool event_throttle(struct vm_event *event)
{
	struct vm_event_proc *proc;
	struct event_throttle_ctl *ctl;
	uint32_t current_rate;
	bool ret = false;

	proc = get_vm_event_proc(event);
	if (proc) {
		ctl = &proc->throttle_ctl;
		if (ctl->is_up) {
			pthread_mutex_lock(&ctl->mtx);
			current_rate = ctl->event_counter / THROTTLE_WINDOW;
			if (current_rate < proc->throttle_rate) {
				ctl->event_counter++;
				ret = false;
			} else {
				ret = true;
				ctl->throttle_count++;
				pr_notice("event %d throttle: %d dropped\n",
					event->type, ctl->throttle_count);
			}
			pthread_mutex_unlock(&ctl->mtx);
		}
	}
	return ret;
}

void throttle_timer_cb(void *arg, uint64_t nexp)
{
	struct event_throttle_ctl *ctl = (struct event_throttle_ctl *)arg;
	pthread_mutex_lock(&ctl->mtx);
	ctl->event_counter = 0;
	pthread_mutex_unlock(&ctl->mtx);
}

static void vm_event_throttle_init(struct vmctx *ctx)
{
	int i;
	struct event_throttle_ctl *ctl;
	int ret = 0;
	struct itimerspec timer_spec;

	for (i = 0; i < ARRAY_SIZE(ve_proc); i++) {
		ctl = &ve_proc[i].throttle_ctl;
		ctl->event_counter = 0U;
		ctl->throttle_count = 0U;
		ctl->is_up = false;
		pthread_mutex_init(&ctl->mtx, NULL);
		ctl->timer.clockid = CLOCK_MONOTONIC;
		ret = acrn_timer_init(&ctl->timer, throttle_timer_cb, ctl);
		if (ret < 0) {
			pr_warn("failed to create timer for vm_event %d, throttle disabled\n", i);
			continue;
		}
		timer_spec.it_value.tv_sec = THROTTLE_WINDOW;
		timer_spec.it_value.tv_nsec = 0;
		timer_spec.it_interval.tv_sec = THROTTLE_WINDOW;
		timer_spec.it_interval.tv_nsec = 0;
		ret = acrn_timer_settime(&ctl->timer, &timer_spec);
		if (ret < 0) {
			pr_warn("failed to set timer for vm_event %d, throttle disabled\n", i);
			continue;
		}
		ctl->is_up = true;
	}
}

static void vm_event_throttle_deinit(void)
{
	int i;
	struct event_throttle_ctl *ctl;

	for (i = 0; i < ARRAY_SIZE(ve_proc); i++) {
		ctl = &ve_proc[i].throttle_ctl;
		if (ctl->timer.fd != -1) {
			acrn_timer_deinit(&ctl->timer);
		}
	}
}

static char *generate_vm_event_message(struct vm_event *event)
{
	char *event_msg = NULL;
	cJSON *val;
	cJSON *event_obj = cJSON_CreateObject();
	struct vm_event_proc *proc;

	if (event_obj == NULL)
		return NULL;
	val = cJSON_CreateNumber(event->type);
	if (val == NULL)
		return NULL;
	cJSON_AddItemToObject(event_obj, "vm_event", val);

	proc = get_vm_event_proc(event);
	if (proc && proc->gen_jdata_handler) {
		(proc->gen_jdata_handler)(event_obj, event);
	}

	event_msg = cJSON_Print(event_obj);
	if (event_msg == NULL)
		fprintf(stderr, "Failed to generate vm_event message.\n");

	cJSON_Delete(event_obj);
	return event_msg;
}

static void emit_vm_event(struct vmctx *ctx, struct vm_event *event)
{
	if (!event_throttle(event)) {
		char *msg = generate_vm_event_message(event);
		if (msg != NULL) {
			vm_monitor_send_vm_event(msg);
			free(msg);
		}
	}
}

static void general_event_handler(struct vmctx *ctx, struct vm_event *event)
{
	emit_vm_event(ctx, event);
}

static void *vm_event_thread(void *param)
{
	int n, i;
	struct vm_event ve;
	eventfd_t val;
	struct vm_event_tunnel *tunnel;
	struct vmctx *ctx = param;

	struct epoll_event eventlist[MAX_EPOLL_EVENTS];

	while (started) {
		n = epoll_wait(epoll_fd, eventlist, MAX_EPOLL_EVENTS, -1);
		if (n < 0) {
			if (errno != EINTR) {
				pr_err("%s: epoll failed %d\n", __func__, errno);
			}
			continue;
		}
		for (i = 0; i < n; i++) {
			if (i < MAX_EPOLL_EVENTS) {
				tunnel = eventlist[i].data.ptr;
				eventfd_read(tunnel->kick_fd, &val);
				if (tunnel && tunnel->enabled) {
					while (!sbuf_is_empty(tunnel->sbuf)) {
						struct vm_event_proc *proc;
						sbuf_get(tunnel->sbuf, (uint8_t*)&ve);
						pr_dbg("%ld vm event from%d %d\n", val, tunnel->type, ve.type);
						proc = get_vm_event_proc(&ve);
						if (proc && proc->ve_handler) {
							(proc->ve_handler)(ctx, &ve);
						} else {
							pr_warn("%s: unhandled vm event type %d\n", __func__, ve.type);
						}

					}
				}
			}
		}
	}
	return NULL;
}

static struct vm_event_tunnel ve_tunnel[MAX_VM_EVENT_TUNNELS] = {
	{
		.type = EVENT_SOURCE_TYPE_HV,
		.sbuf = (struct shared_buf *)hv_vm_event_page,
		.sbuf_size = 4096,
		.enabled = false,
	},
	{
		.type = EVENT_SOURCE_TYPE_DM,
		.sbuf = (struct shared_buf *)dm_vm_event_page,
		.sbuf_size = 4096,
		.enabled = false,
	},
};

static int create_event_tunnel(struct vmctx *ctx, struct vm_event_tunnel *tunnel, int epoll_fd)
{
	struct epoll_event ev;
	enum event_source_type type = tunnel->type;
	struct shared_buf *sbuf = tunnel->sbuf;
	int kick_fd = -1;
	int error;

	sbuf_init(sbuf, tunnel->sbuf_size, VM_EVENT_ELE_SIZE);

	if (type == EVENT_SOURCE_TYPE_HV) {
		error = ioctl(ctx->fd, ACRN_IOCTL_SETUP_VM_EVENT_RING, sbuf);
		if (error) {
			pr_err("%s: Setting vm_event ring failed %d\n", __func__, error);
			goto out;
		}
	}

	kick_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (kick_fd < 0) {
		pr_err("%s: eventfd failed %d\n", __func__, errno);
		goto out;
	}

	if (type == EVENT_SOURCE_TYPE_HV) {
		error = ioctl(ctx->fd, ACRN_IOCTL_SETUP_VM_EVENT_FD, kick_fd);
		if (error) {
			pr_err("%s: Setting vm_event fd failed %d\n", __func__, error);
			goto out;
		}
	}

	ev.events = EPOLLIN;
	ev.data.ptr = tunnel;
	error = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, kick_fd, &ev);
	if (error < 0) {
		pr_err("%s: failed to add fd, error is %d\n", __func__, errno);
		goto out;
	}

	tunnel->kick_fd = kick_fd;
	pthread_mutex_init(&tunnel->mtx, NULL);
	tunnel->enabled = true;

	return 0;

out:
	if (kick_fd >= 0) {
		close(kick_fd);
	}
	return -1;
}

void destory_event_tunnel(struct vm_event_tunnel *tunnel)
{
	if (tunnel->enabled) {
		close(tunnel->kick_fd);
		tunnel->enabled = false;
		pthread_mutex_destroy(&tunnel->mtx);
	}
}

int vm_event_init(struct vmctx *ctx)
{
	int error;

	epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		pr_err("%s: failed to create epoll %d\n", __func__, errno);
		goto out;
	}

	error = create_event_tunnel(ctx, &ve_tunnel[HV_VM_EVENT_TUNNEL], epoll_fd);
	if (error) {
		goto out;
	}

	error = create_event_tunnel(ctx, &ve_tunnel[DM_VM_EVENT_TUNNEL], epoll_fd);
	if (error) {
		goto out;
	}

	vm_event_throttle_init(ctx);

	error = pthread_create(&vm_event_tid, NULL, vm_event_thread, ctx);
	if (error) {
		pr_err("%s: vm_event create failed %d\n", __func__, errno);
		goto out;
	}

	started = true;
	return 0;

out:
	if (epoll_fd >= 0) {
		close(epoll_fd);
	}
	destory_event_tunnel(&ve_tunnel[HV_VM_EVENT_TUNNEL]);
	destory_event_tunnel(&ve_tunnel[DM_VM_EVENT_TUNNEL]);
	return -1;
}

int vm_event_deinit(void)
{
	void *jval;

	if (started) {
		started = false;
		vm_event_throttle_deinit();
		pthread_kill(vm_event_tid, SIGCONT);
		pthread_join(vm_event_tid, &jval);
		close(epoll_fd);
		destory_event_tunnel(&ve_tunnel[HV_VM_EVENT_TUNNEL]);
		destory_event_tunnel(&ve_tunnel[DM_VM_EVENT_TUNNEL]);
	}
	return 0;
}

/* Send a dm generated vm_event by putting it to sbuf.
 * A thread will receive and process those events.
 * Events will be dropped if sbuf is full.
 * They also maight be dropped due to event throttle control in receive thread.
 */
int dm_send_vm_event(struct vm_event *event)
{
	struct vm_event_tunnel *tunnel = &ve_tunnel[DM_VM_EVENT_TUNNEL];
	struct shared_buf *sbuf;
	int32_t ret = -1;
	uint32_t size_sent;

	if (!tunnel->enabled) {
		return -1;
	}
	sbuf = tunnel->sbuf;

	if (sbuf != NULL) {
		pthread_mutex_lock(&tunnel->mtx);
		size_sent = sbuf_put(sbuf, (uint8_t *)event, sizeof(*event));
		pthread_mutex_unlock(&tunnel->mtx);
		if (size_sent == VM_EVENT_ELE_SIZE) {
			eventfd_write(tunnel->kick_fd, 1UL);
			ret = 0;
		}
	}
	return ret;
}
