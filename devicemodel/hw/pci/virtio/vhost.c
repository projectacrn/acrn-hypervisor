/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/vhost.h>

#include "dm.h"
#include "pci_core.h"
#include "irq.h"
#include "vmmapi.h"
#include "vhost.h"

static int vhost_debug;
#define LOG_TAG "vhost: "
#define DPRINTF(fmt, args...) \
	do { if (vhost_debug) printf(LOG_TAG fmt, ##args); } while (0)
#define WPRINTF(fmt, args...) printf(LOG_TAG fmt, ##args)

static void
vhost_kernel_init(struct vhost_dev *vdev, struct virtio_base *base,
		  int fd, int vq_idx, uint32_t busyloop_timeout)
{
	/* to be implemented */
}

static void
vhost_kernel_deinit(struct vhost_dev *vdev)
{
	/* to be implemented */
}

static int
vhost_kernel_set_vring_busyloop_timeout(struct vhost_dev *vdev,
					struct vhost_vring_state *s)
{
	/* to be implemented */
	return -1;
}

static int
vhost_kernel_set_features(struct vhost_dev *vdev,
			  uint64_t features)
{
	/* to be implemented */
	return -1;
}

static int
vhost_kernel_get_features(struct vhost_dev *vdev,
			  uint64_t *features)
{
	/* to be implemented */
	return -1;
}

static int
vhost_kernel_set_owner(struct vhost_dev *vdev)
{
	/* to be implemented */
	return -1;
}

static int
vhost_kernel_reset_device(struct vhost_dev *vdev)
{
	/* to be implemented */
	return -1;
}

static int
vhost_kernel_net_set_backend(struct vhost_dev *vdev,
			     struct vhost_vring_file *file)
{
	/* to be implemented */
	return -1;
}

static int
vhost_vq_init(struct vhost_dev *vdev, int idx)
{
	/* to be implemented */
	return -1;
}

static int
vhost_vq_deinit(struct vhost_vq *vq)
{
	/* to be implemented */
	return -1;
}

static int
vhost_vq_start(struct vhost_dev *vdev, int idx)
{
	/* to be implemented */
	return -1;
}

static int
vhost_vq_stop(struct vhost_dev *vdev, int idx)
{
	/* to be implemented */
	return -1;
}

static int
vhost_set_mem_table(struct vhost_dev *vdev)
{
	/* to be implemented */
	return -1;
}

int
vhost_dev_init(struct vhost_dev *vdev,
	       struct virtio_base *base,
	       int fd,
	       int vq_idx,
	       uint64_t vhost_features,
	       uint64_t vhost_ext_features,
	       uint32_t busyloop_timeout)
{
	uint64_t features;
	int i, rc;

	/* sanity check */
	if (!base || !base->queues || !base->vops) {
		WPRINTF("virtio_base is not initialized\n");
		goto fail;
	}

	if (!vdev->vqs || vdev->nvqs == 0) {
		WPRINTF("virtqueue is not initialized\n");
		goto fail;
	}

	if (vq_idx + vdev->nvqs > base->vops->nvq) {
		WPRINTF("invalid vq_idx: %d\n", vq_idx);
		goto fail;
	}

	vhost_kernel_init(vdev, base, fd, vq_idx, busyloop_timeout);

	rc = vhost_kernel_get_features(vdev, &features);
	if (rc < 0) {
		WPRINTF("vhost_get_features failed\n");
		goto fail;
	}

	for (i = 0; i < vdev->nvqs; i++) {
		rc = vhost_vq_init(vdev, i);
		if (rc < 0)
			goto fail;
	}

	/* specific backend features to vhost */
	vdev->vhost_ext_features = vhost_ext_features & features;

	/* features supported by vhost */
	vdev->vhost_features = vhost_features & features;

	/*
	 * If the features bits are not supported by either vhost kernel
	 * mediator or configuration of device model(specified by
	 * vhost_features), they should be disabled in device_caps,
	 * which expose as virtio host_features for virtio FE driver.
	 */
	vdev->base->device_caps &= ~(vhost_features ^ features);
	vdev->started = false;

	return 0;

fail:
	vhost_dev_deinit(vdev);
	return -1;
}

int
vhost_dev_deinit(struct vhost_dev *vdev)
{
	int i;

	if (!vdev->base || !vdev->base->queues || !vdev->base->vops)
		return -1;

	for (i = 0; i < vdev->nvqs; i++)
		vhost_vq_deinit(&vdev->vqs[i]);

	vhost_kernel_deinit(vdev);

	return 0;
}

int
vhost_dev_start(struct vhost_dev *vdev)
{
	struct vhost_vring_state state;
	uint64_t features;
	int i, rc;

	if (vdev->started)
		return 0;

	/* sanity check */
	if (!vdev->base || !vdev->base->queues || !vdev->base->vops) {
		WPRINTF("virtio_base is not initialized\n");
		goto fail;
	}

	if ((vdev->base->status & VIRTIO_CR_STATUS_DRIVER_OK) == 0) {
		WPRINTF("status error 0x%x\n", vdev->base->status);
		goto fail;
	}

	rc = vhost_kernel_set_owner(vdev);
	if (rc < 0) {
		WPRINTF("vhost_set_owner failed\n");
		goto fail;
	}

	/* set vhost internal features */
	features = (vdev->base->negotiated_caps & vdev->vhost_features) |
		vdev->vhost_ext_features;
	rc = vhost_kernel_set_features(vdev, features);
	if (rc < 0) {
		WPRINTF("set_features failed\n");
		goto fail;
	}
	DPRINTF("set_features: 0x%lx\n", features);

	/* set memory table */
	rc = vhost_set_mem_table(vdev);
	if (rc < 0) {
		WPRINTF("set_mem_table failed\n");
		goto fail;
	}

	/* config busyloop timeout */
	if (vdev->busyloop_timeout) {
		state.num = vdev->busyloop_timeout;
		for (i = 0; i < vdev->nvqs; i++) {
			state.index = i;
			rc = vhost_kernel_set_vring_busyloop_timeout(vdev,
				&state);
			if (rc < 0) {
				WPRINTF("set_busyloop_timeout failed\n");
				goto fail;
			}
		}
	}

	/* start vhost virtqueue */
	for (i = 0; i < vdev->nvqs; i++) {
		rc = vhost_vq_start(vdev, i);
		if (rc < 0)
			goto fail_vq;
	}

	vdev->started = true;
	return 0;

fail_vq:
	while (--i >= 0)
		vhost_vq_stop(vdev, i);
fail:
	return -1;
}

int
vhost_dev_stop(struct vhost_dev *vdev)
{
	int i, rc = 0;

	for (i = 0; i < vdev->nvqs; i++)
		vhost_vq_stop(vdev, i);

	/* the following are done by this ioctl:
	 * 1) resources of the vhost dev are freed
	 * 2) vhost virtqueues are reset
	 */
	rc = vhost_kernel_reset_device(vdev);
	if (rc < 0) {
		WPRINTF("vhost_reset_device failed\n");
		rc = -1;
	}

	vdev->started = false;
	return rc;
}

int
vhost_net_set_backend(struct vhost_dev *vdev, int backend_fd)
{
	struct vhost_vring_file file;
	int rc, i;

	file.fd = backend_fd;
	for (i = 0; i < vdev->nvqs; i++) {
		file.index = i;
		rc = vhost_kernel_net_set_backend(vdev, &file);
		if (rc < 0)
			goto fail;
	}

	return 0;
fail:
	file.fd = -1;
	while (--i >= 0) {
		file.index = i;
		vhost_kernel_net_set_backend(vdev, &file);
	}

	return -1;
}
