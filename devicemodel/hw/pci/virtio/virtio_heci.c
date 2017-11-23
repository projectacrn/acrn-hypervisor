/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * HECI device virtualization.
 */

#include <sys/cdefs.h>
#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"

#define VIRTIO_HECI_RXQ		0
#define VIRTIO_HECI_TXQ		1
#define VIRTIO_HECI_RXSEGS	1
#define VIRTIO_HECI_TXSEGS	2

/*
 * HECI HW max support FIFO depth is 128
 * We might support larger depth, which need change MEI driver
 */
#define VIRTIO_HECI_FIFOSZ	128

#define VIRTIO_HECI_RINGSZ	64
#define VIRTIO_HECI_VQNUM	2

struct virtio_heci_config {
	int	buf_depth;
	uint8_t	hw_ready;
	uint8_t	host_reset;
} __attribute__((packed));

struct virtio_heci {
	struct virtio_base		base;
	struct virtio_vq_info		vqs[VIRTIO_HECI_VQNUM];
	pthread_mutex_t			mutex;

	struct virtio_heci_config	*config;
};

/*
 * Debug printf
 */
static int virtio_heci_debug;
#define DPRINTF(params) do { if (virtio_heci_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

static void virtio_heci_reset(void *);
static void virtio_heci_notify_rx(void *, struct virtio_vq_info *);
static void virtio_heci_notify_tx(void *, struct virtio_vq_info *);
static int virtio_heci_cfgread(void *, int, int, uint32_t *);
static int virtio_heci_cfgwrite(void *, int, int, uint32_t);

static struct virtio_ops virtio_heci_ops = {
	"virtio_heci",		/* our name */
	VIRTIO_HECI_VQNUM,	/* we support several virtqueues */
	sizeof(struct virtio_heci_config), /* config reg size */
	virtio_heci_reset,	/* reset */
	NULL,			/* device-wide qnotify */
	virtio_heci_cfgread,	/* read virtio config */
	virtio_heci_cfgwrite,	/* write virtio config */
	NULL,			/* apply negotiated features */
	NULL,                   /* called on guest set status */
	0,			/* our capabilities */
};

static void
virtio_heci_reset(void *vth)
{
	struct virtio_heci *vheci = vth;

	DPRINTF(("vheci: device reset requested !\r\n"));
	virtio_reset_dev(&vheci->base);
}

static void
virtio_heci_notify_rx(void *heci, struct virtio_vq_info *vq)
{
	/*
	 * Any ring entries to process?
	 */
	if (!vq_has_descs(vq))
		return;
}

static void
virtio_heci_notify_tx(void *heci, struct virtio_vq_info *vq)
{
	/*
	 * Any ring entries to process?
	 */
	if (!vq_has_descs(vq))
		return;
}

static int
virtio_heci_cfgread(void *vsc, int offset, int size, uint32_t *retval)
{
	struct virtio_heci *vheci = vsc;
	void *ptr;

	ptr = (uint8_t *)vheci->config + offset;
	memcpy(retval, ptr, size);
	return 0;
}

static int
virtio_heci_cfgwrite(void *vsc, int offset, int size, uint32_t val)
{
	/* TODO: need handle device config writing */
	return 0;
}

static int
virtio_heci_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_heci *vheci;
	pthread_mutexattr_t attr;
	int i, rc;

	vheci = calloc(1, sizeof(struct virtio_heci));
	if (!vheci) {
		WPRINTF(("vheci init: fail to alloc virtio_heci!\r\n"));
		return -1;
	}
	vheci->config = calloc(1, sizeof(struct virtio_heci_config));
	if (!vheci->config) {
		WPRINTF(("vheci init: fail to alloc virtio_heci_config!\r\n"));
		goto fail;
	}
	vheci->config->buf_depth = VIRTIO_HECI_FIFOSZ;
	vheci->config->hw_ready = 1;

	/* init mutex attribute properly */
	rc = pthread_mutexattr_init(&attr);
	if (rc) {
		WPRINTF(("vheci init: mutexattr init fail, erro %d!\r\n", rc));
		goto fail;
	}
	if (fbsdrun_virtio_msix()) {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
		if (rc) {
			WPRINTF(("vheci init: mutexattr_settype failed with "
				"error %d!\r\n", rc));
			goto fail;
		}
	} else {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		if (rc) {
			WPRINTF(("vheci init: mutexattr_settype failed with "
				"error %d!\r\n", rc));
			goto fail;
		}
	}
	rc = pthread_mutex_init(&vheci->mutex, &attr);
	if (rc) {
		WPRINTF(("vheci init: mutex init failed, error %d!\r\n", rc));
		goto fail;
	}

	virtio_linkup(&vheci->base, &virtio_heci_ops,
			vheci, dev, vheci->vqs);
	vheci->base.mtx = &vheci->mutex;

	for (i = 0; i < VIRTIO_HECI_VQNUM; i++)
		vheci->vqs[i].qsize = VIRTIO_HECI_RINGSZ;
	vheci->vqs[VIRTIO_HECI_RXQ].notify = virtio_heci_notify_rx;
	vheci->vqs[VIRTIO_HECI_TXQ].notify = virtio_heci_notify_tx;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_HECI);
	pci_set_cfgdata16(dev, PCIR_VENDOR, INTEL_VENDOR_ID);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_SIMPLECOMM);
	pci_set_cfgdata8(dev, PCIR_SUBCLASS, PCIS_SIMPLECOMM_OTHER);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_HECI);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, INTEL_VENDOR_ID);

	if (virtio_interrupt_init(&vheci->base, fbsdrun_virtio_msix()))
		goto setup_fail;
	virtio_set_io_bar(&vheci->base, 0);

	return 0;
setup_fail:
	pthread_mutex_destroy(&vheci->mutex);
fail:
	free(vheci->config);
	free(vheci);
	return -1;
}

static void
virtio_heci_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_heci *vheci = (struct virtio_heci *)dev->arg;

	pthread_mutex_destroy(&vheci->mutex);
	free(vheci->config);
	free(vheci);
}

struct pci_vdev_ops pci_ops_vheci = {
	.class_name	= "virtio-heci",
	.vdev_init	= virtio_heci_init,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read,
	.vdev_deinit    = virtio_heci_deinit
};
DEFINE_PCI_DEVTYPE(pci_ops_vheci);
