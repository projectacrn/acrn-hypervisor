/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/*
 * virtio hyper dmabuf
 * Allows to share data buffers between VMs using dmabuf like interface
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"
#include "virtio_kernel.h"
#include "vmmapi.h"

/*
 * Size of queue was chosen experimentaly in a way
 * that it allows to run ~20 shared surfaces without
 * any delays on hyper dmabuf dirver side due to lack
 * of free buffers in queue
 */
#define HYPER_DMABUF_RINGSZ 128

/* Hyper dmabuf uses two queues one for Rx and one for Tx */
#define HYPER_DMABUF_VQ_NUM 2

const char *hyper_dmabuf_vbs_dev_path = "/dev/vbs_hyper_dmabuf";

static int virtio_hyper_dmabuf_debug;
#define DPRINTF(...)\
do {\
	if (virtio_hyper_dmabuf_debug)\
		printf(__VA_ARGS__);\
} while (0)

#define WPRINTF(...) printf(__VA_ARGS__)

static enum VBS_K_STATUS kstatus = VIRTIO_DEV_INITIAL;
static int vbs_k_hyper_dmabuf_fd = -1;
static struct vbs_dev_info kdev;
static struct vbs_vqs_info kvqs;

struct virtio_hyper_dmabuf {
	struct virtio_base base;
	struct virtio_vq_info vq[HYPER_DMABUF_VQ_NUM];
	pthread_mutex_t mtx;
};

static int virtio_hyper_dmabuf_k_init(void);
static int virtio_hyper_dmabuf_k_start(void);
static int virtio_hyper_dmabuf_k_stop(void);
static int virtio_hyper_dmabuf_k_reset(void);
static int virtio_hyper_dmabuf_k_dev_set(const char *name, int vmid,
					 int nvq, uint32_t feature,
					 uint64_t pio_start, uint64_t pio_len);

static int virtio_hyper_dmabuf_k_vq_set(unsigned int nvq, unsigned int idx,
					uint16_t qsize,
					uint32_t pfn, uint16_t msix_idx,
					uint64_t msix_addr, uint32_t msix_data);

static void virtio_hyper_dmabuf_no_notify(void *, struct virtio_vq_info *);
static void virtio_hyper_dmabuf_set_status(void *, uint64_t);
static void virtio_hyper_dmabuf_reset(void *);

static struct virtio_ops virtio_hyper_dmabuf_ops_k = {
	"virtio_hyper_dmabuf",		/* our name */
	HYPER_DMABUF_VQ_NUM,		/* we support 2 virtqueue */
	0,				/* config reg size */
	virtio_hyper_dmabuf_reset,	/* reset */
	virtio_hyper_dmabuf_no_notify,	/* device-wide qnotify */
	NULL,				/* read virtio config */
	NULL,				/* write virtio config */
	NULL,				/* apply negotiated features */
	virtio_hyper_dmabuf_set_status,	/* called on guest set status */
};

static int
virtio_hyper_dmabuf_k_init()
{
	if (vbs_k_hyper_dmabuf_fd != -1) {
		WPRINTF("virtio_hyper_dmabuf: Ooops! Re-entered!!\n");
		return -VIRTIO_ERROR_REENTER;
	}

	vbs_k_hyper_dmabuf_fd = open(hyper_dmabuf_vbs_dev_path, O_RDWR);
	if (vbs_k_hyper_dmabuf_fd < 0) {
		WPRINTF("virtio_hyper_dmabuf: Failed to open %s!\n",
			 hyper_dmabuf_vbs_dev_path);
		return -VIRTIO_ERROR_FD_OPEN_FAILED;
	}
	DPRINTF("virtio_hyper_dmabuf: Open %s success!\n",
		 hyper_dmabuf_vbs_dev_path);

	memset(&kdev, 0, sizeof(kdev));
	memset(&kvqs, 0, sizeof(kvqs));

	return VIRTIO_SUCCESS;
}

static int
virtio_hyper_dmabuf_k_dev_set(const char *name, int vmid, int nvq,
			      uint32_t feature, uint64_t pio_start,
			      uint64_t pio_len)
{
	/* init kdev */
	strncpy(kdev.name, name, VBS_NAME_LEN - 1);
	kdev.name[VBS_NAME_LEN - 1] = 0;
	kdev.vmid = vmid;
	kdev.nvq = nvq;
	kdev.negotiated_features = feature;
	kdev.pio_range_start = pio_start;
	kdev.pio_range_len = pio_len;

	return VIRTIO_SUCCESS;
}

static int
virtio_hyper_dmabuf_k_vq_set(unsigned int nvq, unsigned int idx,
			     uint16_t qsize, uint32_t pfn,
			     uint16_t msix_idx, uint64_t msix_addr,
			     uint32_t msix_data)
{
	if (nvq <= idx) {
		WPRINTF("virtio_hyper_dmabuf: wrong idx for vq_set!\n");
		return -VIRTIO_ERROR_GENERAL;
	}

	/* init kvqs */
	kvqs.nvq = nvq;
	kvqs.vqs[idx].qsize = qsize;
	kvqs.vqs[idx].pfn = pfn;
	kvqs.vqs[idx].msix_idx = msix_idx;
	kvqs.vqs[idx].msix_addr = msix_addr;
	kvqs.vqs[idx].msix_data = msix_data;

	return VIRTIO_SUCCESS;
}

static int
virtio_hyper_dmabuf_k_start(void)
{
	if (vbs_kernel_start(vbs_k_hyper_dmabuf_fd, &kdev, &kvqs) < 0) {
		WPRINTF("virtio_hyper_dmabuf: Failed in vbs_kernel_start!\n");
		return -VIRTIO_ERROR_START;
	}

	DPRINTF("virtio_hyper_dmabuf: vbs_kernel_started!\n");
	return VIRTIO_SUCCESS;
}

static int
virtio_hyper_dmabuf_k_stop(void)
{
	return vbs_kernel_stop(vbs_k_hyper_dmabuf_fd);
}

static int
virtio_hyper_dmabuf_k_reset(void)
{
	memset(&kdev, 0, sizeof(kdev));
	memset(&kvqs, 0, sizeof(kvqs));

	return vbs_kernel_reset(vbs_k_hyper_dmabuf_fd);
}

static void
virtio_hyper_dmabuf_reset(void *base)
{
	struct virtio_hyper_dmabuf *hyper_dmabuf;

	hyper_dmabuf = (struct virtio_hyper_dmabuf *)base;

	DPRINTF("virtio_hyper_dmabuf: device reset requested !\n");
	virtio_reset_dev(&hyper_dmabuf->base);
	if (kstatus == VIRTIO_DEV_STARTED) {
		virtio_hyper_dmabuf_k_stop();
		virtio_hyper_dmabuf_k_reset();
		kstatus = VIRTIO_DEV_INIT_SUCCESS;
	}
}

static void
virtio_hyper_dmabuf_no_notify(void *base, struct virtio_vq_info *vq)
{
}

/*
 * This callback gives us a chance to determine the timings
 * to kickoff VBS-K initialization
 */
static void
virtio_hyper_dmabuf_set_status(void *base, uint64_t status)
{
	struct virtio_hyper_dmabuf *hyper_dmabuf;
	int nvq;
	struct msix_table_entry *mte;
	uint64_t msix_addr = 0;
	uint32_t msix_data = 0;
	int rc, i, j;

	hyper_dmabuf = (struct virtio_hyper_dmabuf *) base;
	nvq = hyper_dmabuf->base.vops->nvq;

	if (kstatus == VIRTIO_DEV_INIT_SUCCESS &&
	    (status & VIRTIO_CONFIG_S_DRIVER_OK)) {
		/* time to kickoff VBS-K side */
		/* init vdev first */
		rc = virtio_hyper_dmabuf_k_dev_set(
				hyper_dmabuf->base.vops->name,
				hyper_dmabuf->base.dev->vmctx->vmid,
				nvq,
				hyper_dmabuf->base.negotiated_caps,
				/* currently we let VBS-K handle
				 * kick register
				 */
				hyper_dmabuf->base.dev->bar[0].addr + 16,
				2);

		for (i = 0; i < nvq; i++) {
			if (hyper_dmabuf->vq[i].msix_idx !=
					VIRTIO_MSI_NO_VECTOR) {
				j = hyper_dmabuf->vq[i].msix_idx;
				mte = &hyper_dmabuf->base.dev->msix.table[j];
				msix_addr = mte->addr;
				msix_data = mte->msg_data;
			}
			rc = virtio_hyper_dmabuf_k_vq_set(
				nvq, i,
				hyper_dmabuf->vq[i].qsize,
				hyper_dmabuf->vq[i].pfn,
				hyper_dmabuf->vq[i].msix_idx,
				msix_addr,
				msix_data);

			if (rc < 0) {
				WPRINTF("virtio_hyper_dmabuf:");
				WPRINTF("kernel_set_vq");
				WPRINTF("failed, i %d ret %d\n", i, rc);
				return;
			}
		}
		rc = virtio_hyper_dmabuf_k_start();
		if (rc < 0) {
			WPRINTF("virtio_hyper_dmabuf:");
			WPRINTF("kernel_start() failed\n");
			kstatus = VIRTIO_DEV_START_FAILED;
		} else {
			kstatus = VIRTIO_DEV_STARTED;
		}
	}
}

static int
virtio_hyper_dmabuf_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_hyper_dmabuf *hyper_dmabuf;

	kstatus = VIRTIO_DEV_PRE_INIT;
	pthread_mutexattr_t attr;
	int rc;

	hyper_dmabuf = calloc(1, sizeof(struct virtio_hyper_dmabuf));
	if (!hyper_dmabuf) {
		WPRINTF(("virtio_hdma: calloc returns NULL\n"));
		return -1;
	}

	/* init mutex attribute properly */
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		DPRINTF("mutexattr init failed with erro %d!\n", rc);

	if (virtio_uses_msix()) {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
		DPRINTF("virtio_msix: mutexattr_settype ");
		DPRINTF("failed with error %d!\n", rc);
	} else {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		DPRINTF("virtio_intx: mutexattr_settype ");
		DPRINTF("failed with error %d!\n", rc);
	}

	rc = pthread_mutex_init(&hyper_dmabuf->mtx, &attr);
	if (rc)
		DPRINTF("mutex init failed with error %d!\n", rc);

	virtio_linkup(&hyper_dmabuf->base,
		      &virtio_hyper_dmabuf_ops_k,
		      hyper_dmabuf,
		      dev,
		      hyper_dmabuf->vq,
		      BACKEND_VBSK);

	rc = virtio_hyper_dmabuf_k_init();
	if (rc < 0) {
		WPRINTF("virtio_hyper_dmabuf: VBS-K ");
		WPRINTF("init failed with error %d!\n", rc);
		kstatus = VIRTIO_DEV_INIT_FAILED;
	} else {
		kstatus = VIRTIO_DEV_INIT_SUCCESS;
	}

	hyper_dmabuf->base.mtx = &hyper_dmabuf->mtx;

	hyper_dmabuf->vq[0].qsize = HYPER_DMABUF_RINGSZ;
	hyper_dmabuf->vq[1].qsize = HYPER_DMABUF_RINGSZ;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_HYPERDMABUF);
	pci_set_cfgdata16(dev, PCIR_VENDOR, INTEL_VENDOR_ID);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_MEMORY);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_HYPERDMABUF);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, INTEL_VENDOR_ID);

	if (virtio_interrupt_init(&hyper_dmabuf->base, virtio_uses_msix())) {
		if (hyper_dmabuf)
			free(hyper_dmabuf);
		return -1;
	}

	virtio_set_io_bar(&hyper_dmabuf->base, 0);

	return 0;
}

static void
virtio_hyper_dmabuf_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	if (kstatus == VIRTIO_DEV_STARTED) {
		DPRINTF("virtio_hyper_dmabuf: deinitializing\n");
		virtio_hyper_dmabuf_k_stop();
		virtio_hyper_dmabuf_k_reset();
		kstatus = VIRTIO_DEV_INITIAL;
		assert(vbs_k_hyper_dmabuf_fd >= 0);
		close(vbs_k_hyper_dmabuf_fd);
		vbs_k_hyper_dmabuf_fd = -1;
	}

	if (dev->arg)
		free((struct virtio_hyper_dmabuf *)dev->arg);
}

struct pci_vdev_ops pci_ops_virtio_hyper_dmabuf = {
	.class_name	= "virtio-hyper_dmabuf",
	.vdev_init	= virtio_hyper_dmabuf_init,
	.vdev_deinit	= virtio_hyper_dmabuf_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_hyper_dmabuf);
