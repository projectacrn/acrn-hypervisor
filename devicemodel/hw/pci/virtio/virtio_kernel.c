/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/* Routines to notify the VBS-K in kernel */

#include <stdio.h>
#include <sys/ioctl.h>
#include "virtio_kernel.h"

static int virtio_kernel_debug;
#define DPRINTF(params) do { if (virtio_kernel_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

static int
vbs_dev_info_set(int fd, void *arg)
{
	return ioctl(fd, VBS_K_SET_DEV, arg);
}

static int
vbs_vqs_info_set(int fd, void *arg)
{
	return ioctl(fd, VBS_K_SET_VQ, arg);
}

/* VBS-K common ops */
/**
 * @brief Virtio kernel module reset.
 *
 * @param fd File descriptor representing virtio backend in kernel module.
 *
 * @return 0 on OK and non-zero on error.
 */
int
vbs_kernel_reset(int fd)
{
	return ioctl(fd, VBS_K_RESET_DEV, NULL);
}

/*
 * We need a way to start/stop vbs_k execution since guest might want to
 * change the configuration of the virtio device after VBS-K has been
 * initialized.
 */
/**
 * @brief Virtio kernel module start.
 *
 * @param fd File descriptor representing virtio backend in kernel module.
 * @param dev Pointer to struct vbs_dev_info.
 * @param vqs Pointer to struct vbs_vqs_info.
 *
 * @return 0 on OK and non-zero on error.
 */
int
vbs_kernel_start(int fd, struct vbs_dev_info *dev, struct vbs_vqs_info *vqs)
{
	int ret;

	if (fd < 0) {
		WPRINTF(("%s: fd < 0\n", __func__));
		return -VIRTIO_ERROR_FD_OPEN_FAILED;
	}

	ret = vbs_dev_info_set(fd, dev);
	if (ret < 0) {
		WPRINTF(("vbs_kernel_set_dev failed: ret %d\n", ret));
		return ret;
	}

	ret = vbs_vqs_info_set(fd, vqs);
	if (ret < 0) {
		WPRINTF(("vbs_kernel_set_vqs failed: ret %d\n", ret));
		return ret;
	}

	return VIRTIO_SUCCESS;
}

/**
 * @brief Virtio kernel module stop.
 *
 * @param fd File descriptor representing virtio backend in kernel module.
 *
 * @return 0 on OK and non-zero on error.
 */
int
vbs_kernel_stop(int fd)
{
	DPRINTF(("%s\n", __func__));
	return VIRTIO_SUCCESS;
}
