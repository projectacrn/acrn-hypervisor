/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/**
 * @file vhost_vsock.h
 */

#ifndef __VHOST_VSOCK_H__
#define __VHOST_VSOCK_H__

#include "virtio.h"
#include "vhost.h"

#define VHOST_VSOCK_RXQ  0
#define VHOST_VSOCK_TXQ  1
#define VHOST_VSOCK_CTLQ 2       /* NB: not yet supported */
#define VHOST_VSOCK_MAXQ 3

#define VHOST_VSOCK_QUEUE_SIZE 128
#define VHOST_F_LOG_ALL 26

#define VHOST_VSOCK_FEATURES \
	(1ULL << VIRTIO_F_NOTIFY_ON_EMPTY) | (1ULL << VIRTIO_RING_F_INDIRECT_DESC) | \
	(1ULL << VIRTIO_RING_F_EVENT_IDX) | (1ULL << VHOST_F_LOG_ALL) | \
	(1ULL << VIRTIO_F_ANY_LAYOUT) | (1ULL << VIRTIO_F_VERSION_1)

#define U32_MAX                 ((uint32_t)~0U)
#define VMADDR_CID_HOST         2

struct virtio_vsock_config {
	uint64_t guest_cid;
}__attribute__((packed));

struct virtio_vsock {
	struct virtio_base base;
	pthread_mutex_t mtx;
	struct virtio_vq_info queues[VHOST_VSOCK_MAXQ];
	struct virtio_vsock_config config;
	struct vhost_vsock *vhost_vsock;
	uint64_t        features;
};

struct vhost_vsock {
	struct vhost_dev vdev;
	struct vhost_vq vqs[VHOST_VSOCK_MAXQ];
	int vhost_fd;
	bool vhost_started;
};
#endif
