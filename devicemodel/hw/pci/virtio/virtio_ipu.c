/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/*
 * virtio ipu
 * IPU mediator DM
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
 * Size of queue was chosen experimentally in a way
 * that it allows to do IPU ISYS MIPI camera capture without
 * any delay for interrupt/msg send, this value should
 * be tuned.
 */
#define VIRTIO_IPU_RINGSZ 1024

/*
 * IPU mediation makes use of 2 VQs.
 * VQ0 for buffer mgmnt & 1 time configuration;
 * VQ1 for interrupt/msg exchange
 */
#define VIRTIO_IPU_VQ_NUM 2

#define IPU_VBS_DEV_PATH "/dev/vbs_ipu"

static int ipu_log_level;
#define TAG "virtio_ipu: "
#define LERR 1
#define LWRN 2
#define LDBG 3
#define IPRINTF(lvl, fmt, args...) \
	do { if (lvl <= ipu_log_level) printf(TAG fmt, ##args); } while (0)

struct virtio_ipu {
	struct virtio_base base;
	struct virtio_vq_info vq[VIRTIO_IPU_VQ_NUM];
	pthread_mutex_t mtx;
	/* VBS-K variables */
	struct {
		enum VBS_K_STATUS ipu_kstatus;
		int ipu_fd;
		struct vbs_dev_info ipu_kdev;
		struct vbs_vqs_info ipu_kvqs;
	} vbs_k;
};

static int virtio_ipu_k_init(struct virtio_ipu *ipu);
static int virtio_ipu_k_start(struct virtio_ipu *ipu);
static int virtio_ipu_k_stop(struct virtio_ipu *ipu);
static int virtio_ipu_k_reset(struct virtio_ipu *ipu);
static int virtio_ipu_k_dev_set(struct vbs_dev_info *ipu_kdev,
				const char *name, int vmid,
				int nvq, uint32_t feature,
				uint64_t pio_start, uint64_t pio_len);

static int virtio_ipu_k_vq_set(struct vbs_vqs_info *ipu_kvqs,
				unsigned int nvq, unsigned int idx,
				uint16_t qsize, uint32_t pfn,
				uint16_t msix_idx, uint64_t msix_addr,
				uint32_t msix_data);

static void virtio_ipu_no_notify(void *, struct virtio_vq_info *);
static void virtio_ipu_set_status(void *, uint64_t);
static void virtio_ipu_reset(void *);

static struct virtio_ops virtio_ipu_ops_k = {
	"virtio_ipu",			/* our name */
	VIRTIO_IPU_VQ_NUM,		/* we support 2 virtqueue */
	0,				/* config reg size */
	virtio_ipu_reset,		/* reset */
	virtio_ipu_no_notify,		/* device-wide qnotify */
	NULL,				/* read virtio config */
	NULL,				/* write virtio config */
	NULL,				/* apply negotiated features */
	virtio_ipu_set_status,		/* called on guest set status */
};

static int
virtio_ipu_k_init(struct virtio_ipu *ipu)
{

	if (ipu->vbs_k.ipu_fd != -1) {
		IPRINTF(LWRN, "Ooops! Re-entered!!\n");
		return -VIRTIO_ERROR_REENTER;
	}

	ipu->vbs_k.ipu_fd = open(IPU_VBS_DEV_PATH, O_RDWR);
	if (ipu->vbs_k.ipu_fd < 0) {
		IPRINTF(LWRN, "Failed to open %s!\n",
			 IPU_VBS_DEV_PATH);
		return -VIRTIO_ERROR_FD_OPEN_FAILED;
	}
	IPRINTF(LDBG, "Open %s success fd:%d!\n",
		 IPU_VBS_DEV_PATH, ipu->vbs_k.ipu_fd);

	memset(&ipu->vbs_k.ipu_kdev, 0, sizeof(struct vbs_dev_info));
	memset(&ipu->vbs_k.ipu_kvqs, 0, sizeof(struct vbs_dev_info));

	return VIRTIO_SUCCESS;
}

static int
virtio_ipu_k_dev_set(struct vbs_dev_info *ipu_kdev,
					const char *name, int vmid, int nvq,
					uint32_t feature, uint64_t pio_start,
					uint64_t pio_len)
{
	/* init kdev */
	strncpy(ipu_kdev->name, name, VBS_NAME_LEN);
	ipu_kdev->vmid = vmid;
	ipu_kdev->nvq = nvq;
	ipu_kdev->negotiated_features = feature;
	ipu_kdev->pio_range_start = pio_start;
	ipu_kdev->pio_range_len = pio_len;

	return VIRTIO_SUCCESS;
}

static int
virtio_ipu_k_vq_set(struct vbs_vqs_info *ipu_kvqs,
					unsigned int nvq, unsigned int idx,
					uint16_t qsize, uint32_t pfn,
					uint16_t msix_idx, uint64_t msix_addr,
					uint32_t msix_data)
{

	if (nvq <= idx) {
		IPRINTF(LWRN, "wrong idx for vq_set!\n");
		return -VIRTIO_ERROR_GENERAL;
	}

	/* init kvqs */
	ipu_kvqs->nvq = nvq;
	ipu_kvqs->vqs[idx].qsize = qsize;
	ipu_kvqs->vqs[idx].pfn = pfn;
	ipu_kvqs->vqs[idx].msix_idx = msix_idx;
	ipu_kvqs->vqs[idx].msix_addr = msix_addr;
	ipu_kvqs->vqs[idx].msix_data = msix_data;

	return VIRTIO_SUCCESS;
}

static int
virtio_ipu_k_start(struct virtio_ipu *ipu)
{

	if (vbs_kernel_start(ipu->vbs_k.ipu_fd,
						&ipu->vbs_k.ipu_kdev,
						&ipu->vbs_k.ipu_kvqs) < 0) {
		IPRINTF(LWRN, "Failed in vbs_kernel_start!\n");
		return -VIRTIO_ERROR_START;
	}

	IPRINTF(LDBG, "vbs_kernel_started!\n");
	return VIRTIO_SUCCESS;
}

static int
virtio_ipu_k_stop(struct virtio_ipu *ipu)
{

	return vbs_kernel_stop(ipu->vbs_k.ipu_fd);
}

static int
virtio_ipu_k_reset(struct virtio_ipu *ipu)
{

	memset(&ipu->vbs_k.ipu_kdev, 0, sizeof(struct vbs_dev_info));
	memset(&ipu->vbs_k.ipu_kvqs, 0, sizeof(struct vbs_vqs_info));

	return vbs_kernel_reset(ipu->vbs_k.ipu_fd);
}

static void
virtio_ipu_reset(void *base)
{

	struct virtio_ipu *ipu;

	ipu = (struct virtio_ipu *)base;

	IPRINTF(LDBG, "device reset requested !\n");
	virtio_reset_dev(&ipu->base);
	if (ipu->vbs_k.ipu_kstatus == VIRTIO_DEV_STARTED) {
		virtio_ipu_k_stop(ipu);
		virtio_ipu_k_reset(ipu);
		ipu->vbs_k.ipu_kstatus = VIRTIO_DEV_INIT_SUCCESS;
	}
}

/* VBS-K interface function implementations */
static void
virtio_ipu_no_notify(void *base, struct virtio_vq_info *vq)
{

	 IPRINTF(LWRN, "VBS-K mode! Should not reach here!!\n");
}

/*
 * This callback gives us a chance to determine the timings
 * to kickoff VBS-K initialization
 */
static void
virtio_ipu_set_status(void *base, uint64_t status)
{

	struct virtio_ipu *ipu;
	int nvq;
	struct msix_table_entry *mte;
	uint64_t msix_addr = 0;
	uint32_t msix_data = 0;
	int rc, i, j;

	IPRINTF(LDBG, "set_status status:%ld\n", status);
	ipu = (struct virtio_ipu *) base;
	nvq = ipu->base.vops->nvq;
	if (ipu->vbs_k.ipu_kstatus == VIRTIO_DEV_INIT_SUCCESS &&
	    (status & VIRTIO_CONFIG_S_DRIVER_OK)) {
		/* time to kickoff VBS-K side */
		/* init vdev first */
		rc = virtio_ipu_k_dev_set(
				&ipu->vbs_k.ipu_kdev,
				ipu->base.vops->name,
				ipu->base.dev->vmctx->vmid,
				nvq,
				ipu->base.negotiated_caps,
				/* currently we let VBS-K handle
				 * kick register
				 */
				ipu->base.dev->bar[0].addr + 16,
				2);

		for (i = 0; i < nvq; i++) {
			if (ipu->vq[i].msix_idx !=
					VIRTIO_MSI_NO_VECTOR) {
				j = ipu->vq[i].msix_idx;
				mte = &ipu->base.dev->msix.table[j];
				msix_addr = mte->addr;
				msix_data = mte->msg_data;
			}
			rc = virtio_ipu_k_vq_set(
				&ipu->vbs_k.ipu_kvqs,
				nvq, i,
				ipu->vq[i].qsize,
				ipu->vq[i].pfn,
				ipu->vq[i].msix_idx,
				msix_addr,
				msix_data);

			if (rc < 0) {
				IPRINTF(LWRN, "kernel_set_vq failed");
				IPRINTF(LWRN, "i %d ret %d\n", i, rc);
				return;
			}
		}
		rc = virtio_ipu_k_start(ipu);
		if (rc < 0) {
			IPRINTF(LWRN, "kernel_start() failed\n");
			ipu->vbs_k.ipu_kstatus = VIRTIO_DEV_START_FAILED;
		} else {
			ipu->vbs_k.ipu_kstatus = VIRTIO_DEV_STARTED;
		}
	}
}

static int
virtio_ipu_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{

	struct virtio_ipu *ipu;

	pthread_mutexattr_t attr;
	int rc;

	ipu = calloc(1, sizeof(struct virtio_ipu));
	if (!ipu) {
		IPRINTF(LWRN, "calloc returns NULL\n");
		return -1;
	}
	ipu->vbs_k.ipu_kstatus = VIRTIO_DEV_INITIAL;
	ipu->vbs_k.ipu_fd = -1;

	/* init mutex attribute properly */
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		IPRINTF(LDBG, "mutexattr init failed with erro %d!\n", rc);

	if (virtio_uses_msix()) {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
		IPRINTF(LDBG, "mutexattr_settype failed with error %d!\n", rc);
	} else {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		IPRINTF(LDBG, "mutexattr_settype failed with error %d!\n", rc);
	}

	rc = pthread_mutex_init(&ipu->mtx, &attr);
	if (rc)
		IPRINTF(LDBG, "mutex init failed with error %d!\n", rc);

	virtio_linkup(&ipu->base,
		      &virtio_ipu_ops_k,
		      ipu,
		      dev,
		      ipu->vq,
		      BACKEND_VBSK);

	rc = virtio_ipu_k_init(ipu);
	if (rc < 0) {
		IPRINTF(LWRN, "VBS-K init failed with error %d!\n", rc);
		ipu->vbs_k.ipu_kstatus = VIRTIO_DEV_INIT_FAILED;
		pthread_mutex_destroy(&ipu->mtx);
		free(ipu);
		return -1;
	}

	ipu->vbs_k.ipu_kstatus = VIRTIO_DEV_INIT_SUCCESS;
	ipu->base.mtx = &ipu->mtx;

	ipu->vq[0].qsize = VIRTIO_IPU_RINGSZ;
	ipu->vq[1].qsize = VIRTIO_IPU_RINGSZ;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_IPU);
	pci_set_cfgdata16(dev, PCIR_VENDOR, INTEL_VENDOR_ID);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_MEMORY);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_IPU);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, INTEL_VENDOR_ID);

	if (virtio_interrupt_init(&ipu->base, virtio_uses_msix())) {
		pthread_mutex_destroy(&ipu->mtx);
		close(ipu->vbs_k.ipu_fd);
		free(ipu);
		return -1;
	}

	virtio_set_io_bar(&ipu->base, 0);

	return 0;
}

static void
virtio_ipu_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{

	struct virtio_ipu *ipu;

	ipu = (struct virtio_ipu *)dev->arg;
	if (!ipu) {
		IPRINTF(LDBG, "is NULL!\n");
		return;
	}

	if (ipu->vbs_k.ipu_kstatus == VIRTIO_DEV_STARTED) {
		IPRINTF(LDBG, "deinitializing\n");
		virtio_ipu_k_stop(ipu);
		virtio_ipu_k_reset(ipu);
		ipu->vbs_k.ipu_kstatus = VIRTIO_DEV_INITIAL;
		assert(ipu->vbs_k.ipu_fd >= 0);
		close(ipu->vbs_k.ipu_fd);
		ipu->vbs_k.ipu_fd = -1;
	}
	pthread_mutex_destroy(&ipu->mtx);
	free(ipu);
}

struct pci_vdev_ops pci_ops_virtio_ipu = {
	.class_name	= "virtio-ipu",
	.vdev_init	= virtio_ipu_init,
	.vdev_deinit	= virtio_ipu_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_ipu);
