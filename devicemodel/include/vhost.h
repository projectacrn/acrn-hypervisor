/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/**
 * @file vhost.h
 *
 * @brief VHOST APIs for ACRN Project
 */

#ifndef __VHOST_H__
#define __VHOST_H__

#include "virtio.h"

/**
 * @brief vhost APIs
 *
 * @addtogroup acrn_virtio
 * @{
 */

struct vhost_vq {
	int kick_fd;		/**< fd of kick eventfd */
	int call_fd;		/**< fd of call eventfd */
	int idx;		/**< index of this vq in vhost dev */
	struct vhost_dev *dev;	/**< pointer to vhost_dev */
};

struct vhost_dev {
	/**
	 * backpointer to virtio_base
	 */
	struct virtio_base *base;

	/**
	 * pointer to vhost_vq array
	 */
	struct vhost_vq *vqs;

	/**
	 * number of virtqueues
	 */
	int nvqs;

	/**
	 * vhost chardev fd
	 */
	int fd;

	/**
	 * first vq's index in virtio_vq_info
	 */
	int vq_idx;

	/**
	 * supported virtio defined features
	 */
	uint64_t vhost_features;

	/**
	 * vhost self-defined internal features bits used for
	 * communicate between vhost user-space and kernel-space modules
	 */
	uint64_t vhost_ext_features;

	/**
	 * vq busyloop timeout in us
	 */
	uint32_t busyloop_timeout;

	/**
	 * whether vhost is started
	 */
	bool started;
};

/**
 * @brief vhost_dev initialization.
 *
 * This interface is called to initialize the vhost_dev. It must be called
 * before the actual feature negotiation with the guest OS starts.
 *
 * @param vdev Pointer to struct vhost_dev.
 * @param base Pointer to struct virtio_base.
 * @param fd fd of the vhost chardev.
 * @param vq_idx The first virtqueue which would be used by this vhost dev.
 * @param vhost_features Subset of vhost features which would be enabled.
 * @param vhost_ext_features Specific vhost internal features to be enabled.
 * @param busyloop_timeout Busy loop timeout in us.
 *
 * @return 0 on success and -1 on failure.
 */
int vhost_dev_init(struct vhost_dev *vdev, struct virtio_base *base, int fd,
		   int vq_idx, uint64_t vhost_features,
		   uint64_t vhost_ext_features, uint32_t busyloop_timeout);

/**
 * @brief vhost_dev cleanup.
 *
 * This interface is called to cleanup the vhost_dev.
 *
 * @param vdev Pointer to struct vhost_dev.
 *
 * @return 0 on success and -1 on failure.
 */
int vhost_dev_deinit(struct vhost_dev *vdev);

/**
 * @brief start vhost data plane.
 *
 * This interface is called to start the data plane in vhost.
 *
 * @param vdev Pointer to struct vhost_dev.
 *
 * @return 0 on success and -1 on failure.
 */
int vhost_dev_start(struct vhost_dev *vdev);

/**
 * @brief stop vhost data plane.
 *
 * This interface is called to stop the data plane in vhost.
 *
 * @param vdev Pointer to struct vhost_dev.
 *
 * @return 0 on success and -1 on failure.
 */
int vhost_dev_stop(struct vhost_dev *vdev);

/**
 * @brief set backend fd of vhost net.
 *
 * This interface is called to set the backend fd (for example tap fd)
 * to vhost.
 *
 * @param vdev Pointer to struct vhost_dev.
 * @param backend_fd fd of backend (for example tap fd).
 *
 * @return 0 on success and -1 on failure.
 */
int vhost_net_set_backend(struct vhost_dev *vdev, int backend_fd);

/**
 * @}
 */
#endif
