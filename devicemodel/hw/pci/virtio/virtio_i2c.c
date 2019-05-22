/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sys/param.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <openssl/md5.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"

/* I2c adapter virtualization architecture
 *
 *                +-----------------------------+
 *                | ACRN DM                     |
 *                |  +----------------------+   |  virtqueue
 *                |  |                      |<--+-----------+
 *                |  | virtio i2c mediator  |   |           |
 *                |  |                      |   |           |
 *                |  +--+-----+-----+-------+   |           |
 *                +-----+-----+-----+-----------+           |
 * User space   +-------+     |     +-----------+           |
 *              v             v                 v           |
 *    +---------+----+  +-----+--------+  +-----+------+    |  +-----------+
 * ---+ /dev/i2c-0   +--+ /dev/i2c-1   +--+ /dev/i2c-n +----+--+UOS:       |
 *    |              |                 |  |            |    |  |/dev/i2c-n |
 *    +----------+---+  +-------+------+  +-----+------+    |  +-----+-----+
 * Kernel space  v              v               v           |        v
 *         +-----+-------+ +----+--------+ +----+--------+  |  +-----+------------+
 *         |i2c adapter 0| |i2c adapter 1| |i2c adapter n|  +->|UOS:              |
 *         |             | |             |               |     |virtio i2c adapter|
 *         +-----+-------+ +-------------+ +-------------+     +------------------+
 * --------------+-----------------------------------------
 * Hardware      +----------+
 *               |          |
 *          bus 0v          v      ....
 *         +-----+---+ +----+----+
 *         |i2c slave| |i2c slave| ....
 *         +---------+ +---------+
 */

static int virtio_i2c_debug=0;
#define VIRTIO_I2C_PREF "virtio_i2c: "
#define DPRINTF(fmt, args...) \
	do { if (virtio_i2c_debug) printf(VIRTIO_I2C_PREF fmt, ##args); } while (0)
#define WPRINTF(fmt, args...) printf(VIRTIO_I2C_PREF fmt, ##args)

/*
 * Per-device struct
 */
struct virtio_i2c {
	struct virtio_base base;
	pthread_mutex_t mtx;
	struct virtio_vq_info vq;
	char ident[256];
};

static void virtio_i2c_reset(void *);
static void virtio_i2c_notify(void *, struct virtio_vq_info *);

static struct virtio_ops virtio_i2c_ops = {
	"virtio_i2c",		/* our name */
	1,			/* we support 1 virtqueue */
	0, /* config reg size */
	virtio_i2c_reset,	/* reset */
	virtio_i2c_notify,	/* device-wide qnotify */
	NULL,	/* read PCI config */
	NULL,	/* write PCI config */
	NULL,			/* apply negotiated features */
	NULL,			/* called on guest set status */
};

static void
virtio_i2c_reset(void *vdev)
{
	struct virtio_i2c *vi2c = vdev;

	DPRINTF("device reset requested !\n");
	virtio_reset_dev(&vi2c->base);
}

static void
virtio_i2c_notify(void *vdev, struct virtio_vq_info *vq)
{
	/* TODO: Add notify logic */
}

static int
virtio_i2c_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	MD5_CTX mdctx;
	u_char digest[16];
	struct virtio_i2c *vi2c;
	pthread_mutexattr_t attr;
	int rc;

	vi2c = calloc(1, sizeof(struct virtio_i2c));
	if (!vi2c) {
		WPRINTF("calloc returns NULL\n");
		return -ENOMEM;
	}

	/* init mutex attribute properly to avoid deadlock */
	rc = pthread_mutexattr_init(&attr);
	if (rc) {
		WPRINTF("mutexattr init failed with erro %d!\n", rc);
		goto mtx_fail;
	}
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc) {
		WPRINTF("mutexattr_settype failed with "
					"error %d!\n", rc);
		goto mtx_fail;
	}

	rc = pthread_mutex_init(&vi2c->mtx, &attr);
	if (rc) {
		WPRINTF("pthread_mutex_init failed with "
					"error %d!\n", rc);
		goto mtx_fail;
	}

	/* init virtio struct and virtqueues */
	virtio_linkup(&vi2c->base, &virtio_i2c_ops, vi2c, dev, &vi2c->vq, BACKEND_VBSU);
	vi2c->base.mtx = &vi2c->mtx;
	vi2c->vq.qsize = 64;

	MD5_Init(&mdctx);
	MD5_Update(&mdctx, "vi2c", strlen("vi2c"));
	MD5_Final(digest, &mdctx);
	rc = snprintf(vi2c->ident, sizeof(vi2c->ident),
		"ACRN--%02X%02X-%02X%02X-%02X%02X", digest[0],
		digest[1], digest[2], digest[3], digest[4],
		digest[5]);
	if (rc < 0) {
		WPRINTF("create ident failed");
		goto fail;
	}
	if (rc >= sizeof(vi2c->ident)) {
		WPRINTF("ident too long\n");
	}

	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_I2C);
	pci_set_cfgdata16(dev, PCIR_VENDOR, INTEL_VENDOR_ID);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_SERIALBUS);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_I2C);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, INTEL_VENDOR_ID);

	if (virtio_interrupt_init(&vi2c->base, virtio_uses_msix())) {
		WPRINTF("failed to init interrupt");
		rc = -1;
		goto fail;
	}
	virtio_set_io_bar(&vi2c->base, 0);
	return 0;

fail:
	pthread_mutex_destroy(&vi2c->mtx);
mtx_fail:
	free(vi2c);
	return rc;
}

static void
virtio_i2c_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_i2c *vi2c;

	if (dev->arg) {
		DPRINTF("deinit\n");
		vi2c = (struct virtio_i2c *) dev->arg;
		pthread_mutex_destroy(&vi2c->mtx);
		free(vi2c);
		dev->arg = NULL;
	}
}

struct pci_vdev_ops pci_ops_virtio_i2c = {
	.class_name		= "virtio-i2c",
	.vdev_init		= virtio_i2c_init,
	.vdev_deinit		= virtio_i2c_deinit,
	.vdev_barwrite		= virtio_pci_write,
	.vdev_barread		= virtio_pci_read,
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_i2c);
