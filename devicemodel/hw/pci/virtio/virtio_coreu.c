/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/*
 * CoreU Virtualization
 *
 * CoreU is for PAVP session management. CoreU contains two parts, the daemon
 * and library used by libpavp. When playback premium content which should be
 * protected in GPU memory, media application calls libpavp which uses MEI to
 * create PAVP session. CoreU daemon has the capability to read GPU registers
 * to know whether the PAVP session status is valid. For now, CoreU is not open
 * source.
 *
 *              +---------------+
 *              |               |
 *   +----------+----------+    |    +------------------+
 *   |       ACRN DM       |    |    |     Media APP    |
 *   | +-----------------+ |    |    +------------------+
 *   | |  CoreU Backend  | |    |              |
 *   | +-----------------+ |    |    +------------------+
 *   +---------------------+    |    |      LibPAVP     |
 *              |               |    +------------------+
 *              |               |              |
 *     +------------------+     |    +------------------+
 *     | CoreU SOS Daemon |     |    | CoreU UOS Daemon |
 *     +------------------+     |    +------------------+
 *                              |
 *    Service OS User Space     |     User OS User Space
 *                              |
 *  --------------------------  |  ---------------------------
 *                              |
 *   Service OS Kernel Space    |    User OS Kernel Space
 *                              |
 *                              |    +------------------+
 *                              |    |  CoreU Frontend  |
 *                              |    +------------------+
 *                              |             |
 *                              +-------------+
 *
 * Above diagram illustrates the CoreU architecture in ACRN. In SOS, CoreU
 * daemon starts upon the system boots. In UOS, CoreU daemon gets the PAVP
 * session status by open/read/write /dev/coreu0 which is created by CoreU
 * frontend, instead of accessing GPU. Then the CoreU frontend sends the
 * requests to the CoreU backend thru virtio mechanism. CoreU backend talks to
 * CoreU SOS daemon to get the PAVP session status.
 *
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <sysexits.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"

#define VIRTIO_COREU_RINGSZ	64
#define COREU_MSG_SIZE		72
#define VIRTIO_COREU_NUMQ	1
#define COREU_SERVICE_NAME	"/var/run/coreu"

/* CoreU raw message */
struct coreu_msg {
	uint8_t bytes[COREU_MSG_SIZE];
};

/* Per-device struct - VBS-U */
struct virtio_coreu {
	struct virtio_base		base;
	struct virtio_vq_info		queues[VIRTIO_COREU_NUMQ];
	pthread_mutex_t			mtx;

	pthread_t rx_tid;
	pthread_mutex_t	rx_mtx;
	pthread_cond_t rx_cond;

	/* socket handle to CoreU daemon */
	int fd;
};

/* VBS-U virtio_ops */
static void virtio_coreu_reset(void *);

static struct virtio_ops virtio_coreu_ops = {
	"virtio_coreu",			/* our name */
	VIRTIO_COREU_NUMQ,		/* we support one virtqueue */
	0,				/* config reg size */
	virtio_coreu_reset,		/* reset */
	NULL,				/* device-wide qnotify */
	NULL,				/* read virtio config */
	NULL,				/* write virtio config */
	NULL,				/* apply negotiated features */
	NULL,				/* called on guest set status */
};

/* Debug printf */
static int virtio_coreu_debug;
#define DPRINTF(params) do { if (virtio_coreu_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

static void
virtio_coreu_reset(void *vdev)
{
	struct virtio_coreu *vcoreu = vdev;

	DPRINTF(("virtio_coreu: device reset requested\n"));
	virtio_reset_dev(&vcoreu->base);
}

static int
send_and_receive(int fd, struct coreu_msg *msg)
{
	uint32_t msg_size = sizeof(struct coreu_msg);
	int ret;

	ret = send(fd, (void *)msg, msg_size, 0);
	if (ret < 0) {
		WPRINTF(("send error\n"));
		return ret;
	}

	ret = recv(fd, (void *)msg, msg_size, 0);
	if (ret < 0) {
		WPRINTF(("recv error\n"));
		return ret;
	}

	if (ret < msg_size) {
		WPRINTF(("received part of the data, %d instead of %d\n",
			ret,
			msg_size));
		return ret;
	}

	return 0;
}

static void *
virtio_coreu_thread(void *param)
{
	struct virtio_coreu *vcoreu = param;
	struct virtio_vq_info *rvq = &vcoreu->queues[0];
	struct iovec iov;
	uint16_t idx;
	int ret;
	struct coreu_msg *msg;

	for (;;) {
		pthread_mutex_lock(&vcoreu->rx_mtx);
		ret = pthread_cond_wait(&vcoreu->rx_cond, &vcoreu->rx_mtx);
		pthread_mutex_unlock(&vcoreu->rx_mtx);

		if (ret)
			break;

		while(vq_has_descs(rvq)) {
			vq_getchain(rvq, &idx, &iov, 1, NULL);

			msg = (struct coreu_msg *)(iov.iov_base);

			ret = send_and_receive(vcoreu->fd, msg);
			if (ret < 0)
			{
				close(vcoreu->fd);
				vcoreu->fd = -1;
			}

			/* release this chain and handle more */
			vq_relchain(rvq, idx, sizeof(struct coreu_msg));
		}

		vq_endchains(rvq, 1);
	}

	pthread_exit(NULL);
}

static int
connect_coreu_daemon()
{
	struct sockaddr_un addr;
	int msg_size;
	int fd;
	int ret;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		WPRINTF(("socket error %d\n", errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, COREU_SERVICE_NAME, sizeof(addr.sun_path));

	ret = connect(fd, &addr, sizeof(struct sockaddr_un));
	if (ret < 0) {
		WPRINTF(("connect error %d\n", errno));
		close(fd);
		return -1;
	}

	msg_size = sizeof(struct coreu_msg);
	ret = setsockopt(fd, SOL_SOCKET,
		SO_RCVLOWAT, &msg_size, sizeof(msg_size));
	if (ret < 0) {
		WPRINTF(("setsockopt error\n"));
		close(fd);
		return -1;
	}
	return fd;
}

static void
virtio_coreu_notify(void *vdev, struct virtio_vq_info *vq)
{
	struct virtio_coreu *vcoreu = vdev;

	/* Any ring entries to process */
	if (!vq_has_descs(vq))
		return;

	vcoreu->fd = (vcoreu->fd < 0) ? connect_coreu_daemon() : vcoreu->fd;
	if (vcoreu->fd < 0)
	{
		WPRINTF(("Invalid CoreU daemon file descriptor\n"));
		return;
	}

	/* Signal the thread for processing */
	pthread_mutex_lock(&vcoreu->rx_mtx);
	pthread_cond_signal(&vcoreu->rx_cond);
	pthread_mutex_unlock(&vcoreu->rx_mtx);
}

static int
virtio_coreu_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_coreu *vcoreu;
	pthread_mutexattr_t attr;
	char tname[MAXCOMLEN + 1];
	int rc;

	vcoreu = calloc(1, sizeof(struct virtio_coreu));
	if (!vcoreu) {
		WPRINTF(("vcoreu init: fail to alloc virtio_coreu\n"));
		return -1;
	}

	/* init mutex attribute properly */
	int mutexattr_type = virtio_uses_msix()
			? PTHREAD_MUTEX_DEFAULT
			: PTHREAD_MUTEX_RECURSIVE;

	rc = pthread_mutexattr_init(&attr);
	if (rc)
		WPRINTF(("vcoreu init: mutexattr init fail, erro %d\n", rc));
	rc = pthread_mutexattr_settype(&attr, mutexattr_type);
	if (rc)
		WPRINTF(("vcoreu init: mutexattr_settype fail, erro %d\n", rc));
	rc = pthread_mutex_init(&vcoreu->mtx, &attr);
	if (rc)
		WPRINTF(("vcoreu init: mutexattr_settype fail, erro %d\n", rc));

	DPRINTF(("vcoreu init: using VBS-U...\n"));
	virtio_linkup(&vcoreu->base, &virtio_coreu_ops,
			vcoreu, dev, vcoreu->queues, BACKEND_VBSU);
	vcoreu->base.mtx = &vcoreu->mtx;

	vcoreu->queues[0].qsize  = VIRTIO_COREU_RINGSZ;
	vcoreu->queues[0].notify = virtio_coreu_notify;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_COREU);
	pci_set_cfgdata16(dev, PCIR_VENDOR, INTEL_VENDOR_ID);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_CRYPTO);
	pci_set_cfgdata8(dev, PCIR_SUBCLASS, PCIS_SIMPLECOMM_OTHER);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_COREU);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, INTEL_VENDOR_ID);

	if (virtio_interrupt_init(&vcoreu->base, virtio_uses_msix())) {
		WPRINTF(("vcoreu init: interrupt init fail\n"));
		free(vcoreu);
		return -1;
	}

	virtio_set_io_bar(&vcoreu->base, 0);

	/*
	 * connect to coreu daemon in init phase
	 *
	 * @FIXME if failed connecting to CoreU daemon, the return value should
	 * be set appropriately for SOS not exposing the CoreU PCI device to UOS
	 */
	vcoreu->fd = connect_coreu_daemon();
	if (vcoreu->fd < 0) {
		WPRINTF(("connection to server failed\n"));
		pthread_mutex_destroy(&vcoreu->mtx);
		free(vcoreu);
		return -errno;
	}

	pthread_mutex_init(&vcoreu->rx_mtx, NULL);
	pthread_cond_init(&vcoreu->rx_cond, NULL);
	pthread_create(&vcoreu->rx_tid, NULL,
			virtio_coreu_thread, (void *)vcoreu);
	snprintf(tname, sizeof(tname), "vtcoreu-%d:%d tx",
			dev->slot, dev->func);
	pthread_setname_np(vcoreu->rx_tid, tname);

	return 0;
}

static void
virtio_coreu_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_coreu *vcoreu = (struct virtio_coreu *)dev->arg;

	if (!vcoreu)
		return;

	pthread_mutex_destroy(&vcoreu->mtx);
	pthread_mutex_destroy(&vcoreu->rx_mtx);
	pthread_cond_destroy(&vcoreu->rx_cond);
	pthread_join(vcoreu->rx_tid, NULL);

	free(vcoreu);
}

struct pci_vdev_ops pci_ops_virtio_coreu = {
	.class_name		= "virtio-coreu",
	.vdev_init		= virtio_coreu_init,
	.vdev_deinit		= virtio_coreu_deinit,
	.vdev_barwrite		= virtio_pci_write,
	.vdev_barread		= virtio_pci_read
};

DEFINE_PCI_DEVTYPE(pci_ops_virtio_coreu);
