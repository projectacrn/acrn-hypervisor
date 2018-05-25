/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/* This file provides common routines to interface with VBS-K kernel modules */

#ifndef _VIRTIO_KERNEL_H_
#define _VIRTIO_KERNEL_H_

#include "vbs_common_if.h"		/* data format between VBS-U & VBS-K */

enum VBS_K_STATUS {
	VIRTIO_DEV_INITIAL = 1,		/* initial status */
	VIRTIO_DEV_PRE_INIT,		/* detected thru cmdline option */
	VIRTIO_DEV_INIT_FAILED,		/* init failed */
	VIRTIO_DEV_INIT_SUCCESS,	/* init success */
	VIRTIO_DEV_START_FAILED,	/* start failed */
	VIRTIO_DEV_STARTED,		/* start success */
};

/* Return codes */
#define VIRTIO_SUCCESS				0
#define VIRTIO_ERROR_REENTER			1
#define VIRTIO_ERROR_FD_OPEN_FAILED		2
#define VIRTIO_ERROR_MEM_ALLOC_FAILED		3
#define VIRTIO_ERROR_START			4
#define VIRTIO_ERROR_GENERAL			5

/* VBS-K common ops */
/* VBS-K init/reset*/
int vbs_kernel_init(int fd);
int vbs_kernel_reset(int fd);

/* VBS-K start/stop */
int vbs_kernel_start(int fd, struct vbs_dev_info *dev,
		     struct vbs_vqs_info *vqs);
int vbs_kernel_stop(int fd);

#endif
