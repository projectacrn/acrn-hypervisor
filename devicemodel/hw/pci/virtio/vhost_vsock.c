/*
 * Copyright (C) OASIS Open 2018. All rights reserved.
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * vhost-vsock device
 *
 */

#include <sys/uio.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <pthread.h>
#include <linux/vhost.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"
#include "vhost.h"
#include "dm_string.h"
#include "vhost_vsock.h"

static int
vhost_vsock_set_running(struct vhost_dev *vdev, int start)
{
	return vhost_kernel_ioctl(vdev, VHOST_VSOCK_SET_RUNNING, &start);
}

static int
vhost_vsock_set_guest_cid(struct vhost_dev *vdev,
                                            uint64_t guest_cid)
{
	return vhost_kernel_ioctl(vdev,
		VHOST_VSOCK_SET_GUEST_CID, &guest_cid);
}

static int
vhost_vsock_start(struct vhost_vsock *vhost_vsock)
{
	int rc;

	if (vhost_vsock->vhost_started) {
		pr_err("The vhost vsock is already started, re-using it.\n");
		return 0;
	}

	rc = vhost_dev_start(&vhost_vsock->vdev);
	if (rc < 0) {
		pr_err("vhost_dev_start is failed.\n");
		return -1;
	}

	rc = vhost_vsock_set_running(&vhost_vsock->vdev, 1);
	if (rc < 0) {
		vhost_dev_stop(&vhost_vsock->vdev);
		pr_err("vhost_vsock_set_running is failed with %d.\n", rc);
		return -1;
	}

	vhost_vsock->vhost_started = true;
	return 0;
}

static int
vhost_vsock_stop(struct vhost_vsock *vhost_vsock)
{
	int rc;

	if (!vhost_vsock->vhost_started) {
		pr_err("vhost vsock is not started.\n");
		return 0;
	}

	rc = vhost_vsock_set_running(&vhost_vsock->vdev, 0);
	if (rc < 0) {
		pr_err("vhost_vsock_set_running is failed with %d\n", rc);
		return -1;
	}

	rc = vhost_dev_stop(&vhost_vsock->vdev);
	if (rc < 0) {
		pr_err("vhost_dev_stop is failed.\n");
		return -1;
	}

	vhost_vsock->vhost_started = false;
	return 0;
}

static void
virtio_vsock_set_status(void *vdev, uint64_t status)
{
	struct virtio_vsock *vsock = vdev;
	bool should_start = status & VIRTIO_CONFIG_S_DRIVER_OK;
	int rc;

	if (!vsock->vhost_vsock) {
		pr_err("virtio_vsock_set_status vhost is NULL.\n");
		return;
	}

	if (should_start) {
		rc = vhost_vsock_start(vsock->vhost_vsock);
		if (rc < 0) {
			pr_err("vhost_vsock_start is failed.\n");
			return;
		}
	} else if (vsock->vhost_vsock->vhost_started &&
		should_start == 0) {
		rc = vhost_vsock_stop(vsock->vhost_vsock);
		if (rc < 0) {
			pr_err("vhost_vsock_stop is failed.\n");
			return;
		}
	}
}

static void
virtio_vsock_apply_feature(void *vdev, uint64_t negotiated_features)
{
	struct virtio_vsock *vsock = vdev;

	vsock->features = negotiated_features;
}

static int
virtio_vsock_read_cfg(void *vdev, int offset, int size, uint32_t *retval)
{
	struct virtio_vsock *vsock = vdev;
	void *ptr;

	ptr = (uint8_t *)&vsock->config + offset;
	memcpy(retval, ptr, size);

	return 0;
}

static void
virtio_vsock_reset(void *vdev)
{
	struct virtio_vsock *vsock = vdev;

	pr_dbg(("vsock: device reset requested.\n"));
	/* now reset rings, MSI-X vectors, and negotiated capabilities */
	virtio_reset_dev(&vsock->base);
}

static struct virtio_ops virtio_vsock_ops = {
	"vhost-vsock",			/* our name */
	VHOST_VSOCK_MAXQ,		/* we currently support 2 virtqueues */
	sizeof(struct virtio_vsock_config), /* config reg size */
	virtio_vsock_reset,		/* reset */
	NULL,				/* device-wide qnotify -- not used */
	virtio_vsock_read_cfg,		/* read PCI config */
	NULL,				/* write PCI config */
	virtio_vsock_apply_feature,	/* apply negotiated features */
	virtio_vsock_set_status,	/* called on guest set status */
};

static struct vhost_vsock *
vhost_vsock_init(struct virtio_base *base, int vq_idx)
{
	struct vhost_vsock *vhost_vsock = NULL;
	uint64_t vhost_features = VHOST_VSOCK_FEATURES;
	int rc;

	vhost_vsock = calloc(1, sizeof(struct vhost_vsock));
	if (!vhost_vsock) {
		pr_err(("vhost init out of memory.\n"));
		goto fail;
	}

	/* pre-init before calling vhost_dev_init */
	vhost_vsock->vdev.nvqs = 2;
	vhost_vsock->vdev.vqs = vhost_vsock->vqs;
	vhost_vsock->vhost_fd = open("/dev/vhost-vsock", O_RDWR);;
	if (vhost_vsock->vhost_fd < 0) {
		pr_err(("Open vhost-vsock fail, pls open vsock kernel config.\n"));
		goto fail;
	}
	rc = fcntl(vhost_vsock->vhost_fd, F_GETFL);
	if (rc == -1) {
		pr_err(("fcntl vhost node fail.\n"));
		goto fail;
	}
	if (fcntl(vhost_vsock->vhost_fd, F_SETFL, rc | O_NONBLOCK) == -1) {
		pr_err(("fcntl set NONBLOCK fail.\n"));
		goto fail;
	}

	rc = vhost_dev_init(&vhost_vsock->vdev, base, vhost_vsock->vhost_fd, vq_idx,
		vhost_features, 0, 0);
	if (rc < 0) {
		pr_err(("vhost_dev_init failed.\n"));
		goto fail;
	}

	return vhost_vsock;
fail:
	if (vhost_vsock)
		free(vhost_vsock);
	return NULL;
}

static void
vhost_vsock_deinit(struct vhost_vsock *vhost_vsock)
{
	int rc;

	rc = vhost_vsock_stop(vhost_vsock);
	if (rc < 0)
		pr_err("vhost_dev_stop is failed.\n");

	rc = vhost_dev_deinit(&vhost_vsock->vdev);
	if (rc < 0)
		pr_err("vhost_dev_deinit is failed.\n");

	close(vhost_vsock->vhost_fd);
	free(vhost_vsock);
}

static void
vhost_vsock_handle_output(void *vdev, struct virtio_vq_info *vq) {
	/* do nothing */
}

static int
virtio_vhost_vsock_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_vsock *vsock;
	int rc;
	uint64_t cid = 0;
	pthread_mutexattr_t attr;
	char *devopts = NULL;
	char *tmp = NULL;

	if (opts == NULL) {
		pr_err(("vsock: must have a valid guest_cid.\n"));
		return -1;
	}
	devopts = tmp = strdup(opts);
	if (!devopts) {
		pr_err(("vsock: The vsock parameter is NULL.\n"));
		return -1;
	}
	if (!strncmp(tmp, "cid=", 4)) {
		strsep(&tmp, "=");
		dm_strtoul(tmp, NULL, 10, &cid);
	}
	free(devopts);

	if (cid <= VMADDR_CID_HOST || cid >= U32_MAX) {
		pr_err("vsock: guest_cid has to be 0x2~0xffffffff.\n");
		return -1;
	}

	vsock = calloc(1, sizeof(struct virtio_vsock));
	if (!vsock) {
		pr_err("vosck: memory allocate failed.");
		return -1;
	}

	vsock->config.guest_cid = cid;

	rc = pthread_mutexattr_init(&attr);
	if (rc)
		pr_err("vsock: mutexattr init failed with erro %d\n", rc);
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc)
		pr_err("vsock: mutexattr_settype failed with error %d\n", rc);
	rc = pthread_mutex_init(&vsock->mtx, &attr);
	if (rc)
		pr_err("vsock: pthread_mutexattr_init failed with error %d\n", rc);

	virtio_linkup(&vsock->base, &virtio_vsock_ops, vsock, dev, vsock->queues, BACKEND_VHOST);
	vsock->base.mtx = &vsock->mtx;
	vsock->base.device_caps = (1UL << VIRTIO_F_VERSION_1) | VHOST_VSOCK_FEATURES;

	vsock->queues[VHOST_VSOCK_RXQ].qsize = VHOST_VSOCK_QUEUE_SIZE;
	vsock->queues[VHOST_VSOCK_RXQ].notify = vhost_vsock_handle_output;
	vsock->queues[VHOST_VSOCK_TXQ].qsize = VHOST_VSOCK_QUEUE_SIZE;
	vsock->queues[VHOST_VSOCK_TXQ].notify = vhost_vsock_handle_output;
	vsock->queues[VHOST_VSOCK_CTLQ].qsize = VHOST_VSOCK_QUEUE_SIZE;
	vsock->queues[VHOST_VSOCK_CTLQ].notify = vhost_vsock_handle_output;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_VSOCK);
	pci_set_cfgdata16(dev, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_NETWORK);
	pci_set_cfgdata16(dev, PCIR_REVID, 1);

	virtio_set_modern_bar(&vsock->base, false);

	vsock->vhost_vsock = vhost_vsock_init(&vsock->base, 0);
	if (!vsock->vhost_vsock) {
		pr_err("vhost vosck init failed.");
		free(vsock);
		return -1;
	}
	vhost_vsock_set_guest_cid(&vsock->vhost_vsock->vdev, vsock->config.guest_cid);

	if (virtio_interrupt_init(&vsock->base, virtio_uses_msix())) {
		vhost_vsock_deinit(vsock->vhost_vsock);
		free(vsock);
		return -1;
	}
	return 0;
}

static void
virtio_vhost_vsock_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_vsock *vsock;

	if (dev->arg) {
		vsock = (struct virtio_vsock *) dev->arg;

		if (vsock->vhost_vsock)
			vhost_vsock_deinit(vsock->vhost_vsock);
		pr_dbg("%s: done\n", __func__);
		free(vsock);
	} else
		pr_err("%s: NULL.\n", __func__);
}

struct pci_vdev_ops pci_ops_vhost_vsock = {
	.class_name	= "vhost-vsock",
	.vdev_init	= virtio_vhost_vsock_init,
	.vdev_deinit	= virtio_vhost_vsock_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read
};
DEFINE_PCI_DEVTYPE(pci_ops_vhost_vsock);
