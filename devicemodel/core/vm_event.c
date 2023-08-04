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

#define VM_EVENT_ELE_SIZE (sizeof(struct vm_event))

#define HV_VM_EVENT_TUNNEL 0
#define DM_VM_EVENT_TUNNEL 1
#define MAX_VM_EVENT_TUNNELS 2
#define MAX_EPOLL_EVENTS MAX_VM_EVENT_TUNNELS

typedef void (*vm_event_handler)(struct vmctx *ctx, struct vm_event *event);

static int epoll_fd;
static bool started = false;
static char hv_vm_event_page[4096] __aligned(4096);
static char dm_vm_event_page[4096] __aligned(4096);

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
	if (started) {
		close(epoll_fd);
		destory_event_tunnel(&ve_tunnel[HV_VM_EVENT_TUNNEL]);
		destory_event_tunnel(&ve_tunnel[DM_VM_EVENT_TUNNEL]);
		started = false;
	}
	return 0;
}
