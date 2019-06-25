/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/*
 * virtio audio
 * audio mediator device model
 */

#include <err.h>
#include <errno.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sysexits.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"
#include "virtio_kernel.h"
#include "vmmapi.h"			/* for vmctx */

/*
 * Size of queue was chosen experimentaly in a way
 * that it allows to do audio capture/playback without
 * any delay for interrupt/msg send, this value should
 * be tuned.
 */
#define VIRTIO_AUDIO_RINGSZ	1024

/*
 * Queue definitions.
 * Audio mediator uses two queues: one for interrupt and the other for messages.
 */
#define VIRTIO_AUDIO_VQ_NUM  4 /*4 currently we use 4 vq, may change later*/

const char *vbs_k_audio_dev_path = "/dev/vbs_k_audio";

static int virtio_audio_debug = 1;
#define DPRINTF(params) do { if (virtio_audio_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

struct virtio_audio {
	struct virtio_base base;
	struct virtio_vq_info vq[VIRTIO_AUDIO_VQ_NUM];
	pthread_mutex_t mtx;
	/* VBS-K variables */
	struct {
		enum VBS_K_STATUS kstatus;
		int audio_fd;
		struct vbs_dev_info kdev;
		struct vbs_vqs_info kvqs;
	} vbs_k;
};

static int virtio_audio_kernel_start(struct virtio_audio *virt_audio);
static int virtio_audio_kernel_stop(struct virtio_audio *virt_audio);
static int virtio_audio_kernel_reset(struct virtio_audio *virt_audio);
static int virtio_audio_kernel_dev_set(struct vbs_dev_info *kdev,
				       const char *name, int vmid, int nvq,
				       uint32_t feature, uint64_t pio_start,
				       uint64_t pio_len);
static int virtio_audio_kernel_vq_set(struct vbs_vqs_info *kvqs,
				      unsigned int nvq, unsigned int idx,
				      uint16_t qsize, uint32_t pfn,
				      uint16_t msix_idx, uint64_t msix_addr,
				      uint32_t msix_data);

static void virtio_audio_k_no_notify(void *base, struct virtio_vq_info *vq);
static void virtio_audio_k_set_status(void *base, uint64_t status);
static void virtio_audio_reset(void *base);

static struct virtio_ops virtio_audio_ops_k = {
	"virtio_audio",		/* our name */
	VIRTIO_AUDIO_VQ_NUM,	/* we support 4 virtqueue */
	0,			/* config reg size */
	virtio_audio_reset,	/* reset */
	virtio_audio_k_no_notify,	/* device-wide qnotify */
	NULL,				/* read virtio config */
	NULL,				/* write virtio config */
	NULL,				/* apply negotiated features */
	virtio_audio_k_set_status,/* called on guest set status */
};

static int
virtio_audio_kernel_init(struct virtio_audio *virt_audio)
{
	if (virt_audio->vbs_k.audio_fd != -1) {
		WPRINTF(("virtio_audio: Ooops! Re-entered!!\n"));
		return -VIRTIO_ERROR_REENTER;
	}

	virt_audio->vbs_k.audio_fd = open(vbs_k_audio_dev_path, O_RDWR);
	if (virt_audio->vbs_k.audio_fd < 0) {
		WPRINTF(("virtio_audio: Failed to open %s!\n",
			 vbs_k_audio_dev_path));
		return -VIRTIO_ERROR_FD_OPEN_FAILED;
	}
	DPRINTF(("virtio_audio: Open %s success!\n",
		 vbs_k_audio_dev_path));

	memset(&virt_audio->vbs_k.kdev, 0, sizeof(struct vbs_dev_info));
	memset(&virt_audio->vbs_k.kvqs, 0, sizeof(struct vbs_vqs_info));

	return VIRTIO_SUCCESS;
}

static int
virtio_audio_kernel_dev_set(struct vbs_dev_info *kdev, const char *name,
			    int vmid, int nvq, uint32_t feature,
			    uint64_t pio_start,
			    uint64_t pio_len)
{
	/* init kdev */
	strncpy(kdev->name, name, VBS_NAME_LEN);
	kdev->name[VBS_NAME_LEN - 1] = '\0';
	kdev->vmid = vmid;
	kdev->nvq = nvq;
	kdev->negotiated_features = feature;
	kdev->pio_range_start = pio_start;
	kdev->pio_range_len = pio_len;

	return VIRTIO_SUCCESS;
}

static int
virtio_audio_kernel_vq_set(struct vbs_vqs_info *kvqs, unsigned int nvq,
			   unsigned int idx, uint16_t qsize,
			   uint32_t pfn, uint16_t msix_idx, uint64_t msix_addr,
			   uint32_t msix_data)
{
	if (nvq <= idx) {
		WPRINTF(("virtio_audio: wrong idx for vq_set!\n"));
		return -VIRTIO_ERROR_GENERAL;
	}

	/* init kvqs */
	kvqs->nvq = nvq;
	kvqs->vqs[idx].qsize = qsize;
	kvqs->vqs[idx].pfn = pfn;
	kvqs->vqs[idx].msix_idx = msix_idx;
	kvqs->vqs[idx].msix_addr = msix_addr;
	kvqs->vqs[idx].msix_data = msix_data;

	return VIRTIO_SUCCESS;
}

static int
virtio_audio_kernel_start(struct virtio_audio *virt_audio)
{
	if (vbs_kernel_start(virt_audio->vbs_k.audio_fd,
			     &virt_audio->vbs_k.kdev,
			     &virt_audio->vbs_k.kvqs) < 0) {
		WPRINTF(("virtio_audio: Failed in vbs_k_start!\n"));
		return -VIRTIO_ERROR_START;
	}

	DPRINTF(("virtio_audio: vbs_k_started!\n"));
	return VIRTIO_SUCCESS;
}

static int
virtio_audio_kernel_stop(struct virtio_audio *virt_audio)
{
	return vbs_kernel_stop(virt_audio->vbs_k.audio_fd);
}

static int
virtio_audio_kernel_reset(struct virtio_audio *virt_audio)
{
	memset(&virt_audio->vbs_k.kdev, 0, sizeof(struct vbs_dev_info));
	memset(&virt_audio->vbs_k.kvqs, 0, sizeof(struct vbs_vqs_info));

	return vbs_kernel_reset(virt_audio->vbs_k.audio_fd);
}

static void
virtio_audio_reset(void *base)
{
	struct virtio_audio *virt_audio;

	virt_audio = (struct virtio_audio *)base;

	DPRINTF(("virtio_audio: device reset requested !\n"));
	virtio_reset_dev(&virt_audio->base);
	DPRINTF(("virtio_audio: kstatus %d\n", virt_audio->vbs_k.kstatus));
	if (virt_audio->vbs_k.kstatus == VIRTIO_DEV_STARTED) {
		DPRINTF(("virtio_audio: VBS-K reset requested!\n"));
		virtio_audio_kernel_stop(virt_audio);
		virtio_audio_kernel_reset(virt_audio);
		virt_audio->vbs_k.kstatus = VIRTIO_DEV_INIT_SUCCESS;
	}
}

/* VBS-K interface function implementations */
static void
virtio_audio_k_no_notify(void *base, struct virtio_vq_info *vq)
{
	WPRINTF(("virtio_audio: VBS-K mode! Should not reach here!!\n"));
}

/*
 * This callback gives us a chance to determine the timings
 * to kickoff VBS-K initialization
 */
static void
virtio_audio_k_set_status(void *base, uint64_t status)
{
	struct virtio_audio *virt_audio;
	int nvq;
	struct msix_table_entry *mte;
	uint64_t msix_addr = 0;
	uint32_t msix_data = 0;
	int rc, i, j;

	virt_audio = (struct virtio_audio *)base;
	nvq = virt_audio->base.vops->nvq;

	if (virt_audio->vbs_k.kstatus == VIRTIO_DEV_INIT_SUCCESS &&
	    (status & VIRTIO_CONFIG_S_DRIVER_OK)) {
		/* time to kickoff VBS-K side */
		/* init vdev first */
		rc = virtio_audio_kernel_dev_set(
					&virt_audio->vbs_k.kdev,
					virt_audio->base.vops->name,
					virt_audio->base.dev->vmctx->vmid,
					nvq,
					virt_audio->base.negotiated_caps,
					/* currently we let VBS-K handle
					 * kick register
					 *
					 * FIXME: the size should be returned
					 *  by a api in vhost.
					 */
					virt_audio->base.dev->bar[0].addr + 16,
					2);

		for (i = 0; i < nvq; i++) {
			if (virt_audio->vq[i].msix_idx
				!= VIRTIO_MSI_NO_VECTOR) {
				j = virt_audio->vq[i].msix_idx;
				mte = &virt_audio->base.dev->msix.table[j];
				msix_addr = mte->addr;
				msix_data = mte->msg_data;
			}
			rc = virtio_audio_kernel_vq_set(
				&virt_audio->vbs_k.kvqs,
				nvq, i,
				virt_audio->vq[i].qsize,
				virt_audio->vq[i].pfn,
				virt_audio->vq[i].msix_idx,
				msix_addr,
				msix_data);

			if (rc < 0) {
				WPRINTF(("audio: kernel_set_vq failed, "
					 "i %d ret %d\n", i, rc));
				return;
			}
		}
		rc = virtio_audio_kernel_start(virt_audio);
		if (rc < 0) {
			WPRINTF(("virtio_audio: kernel_start() failed\n"));
			virt_audio->vbs_k.kstatus = VIRTIO_DEV_START_FAILED;
		} else {
			virt_audio->vbs_k.kstatus = VIRTIO_DEV_STARTED;
		}
	}
}

static int
virtio_audio_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_audio *virt_audio;

	pthread_mutexattr_t attr;
	int rc;

	virt_audio = calloc(1, sizeof(struct virtio_audio));
	if (!virt_audio) {
		WPRINTF(("virtio_audio: calloc returns NULL\n"));
		return -1;
	}
	virt_audio->vbs_k.kstatus = VIRTIO_DEV_INITIAL;
	virt_audio->vbs_k.audio_fd = -1;

	/* init mutex attribute properly */
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		DPRINTF(("mutexattr init failed with erro %d!\n", rc));

	if (virtio_uses_msix()) {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
		if (rc)
			DPRINTF(("virtio_msix: mutexattr_settype "
				 "failed with error %d!\n", rc));
	} else {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		if (rc)
			DPRINTF(("virtio_intx: mutexattr_settype "
				 "failed with error %d!\n", rc));
	}

	rc = pthread_mutex_init(&virt_audio->mtx, &attr);
	if (rc)
		DPRINTF(("mutex init failed with error %d!\n", rc));

	virtio_linkup(&virt_audio->base,
		      &virtio_audio_ops_k,
		      virt_audio,
		      dev,
		      virt_audio->vq,
		      BACKEND_VBSK);

	rc = virtio_audio_kernel_init(virt_audio);
	if (rc < 0) {
		WPRINTF(("virtio_audio: VBS-K init failed,error %d!\n", rc));
		virt_audio->vbs_k.kstatus = VIRTIO_DEV_INIT_FAILED;
		free(virt_audio);
		return -1;
	}
	virt_audio->vbs_k.kstatus = VIRTIO_DEV_INIT_SUCCESS;
	virt_audio->base.mtx = &virt_audio->mtx;

	/* vq[0] and vq[1] are for interrupt and messages */
	virt_audio->vq[0].qsize = VIRTIO_AUDIO_RINGSZ;
	virt_audio->vq[1].qsize = VIRTIO_AUDIO_RINGSZ;
	virt_audio->vq[2].qsize = VIRTIO_AUDIO_RINGSZ;
	virt_audio->vq[3].qsize = VIRTIO_AUDIO_RINGSZ;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_AUDIO);
	pci_set_cfgdata16(dev, PCIR_VENDOR, INTEL_VENDOR_ID);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_MULTIMEDIA);
	pci_set_cfgdata8(dev, PCIR_SUBCLASS, PCIS_MULTIMEDIA_AUDIO);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_AUDIO);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, INTEL_VENDOR_ID);

	if (virtio_interrupt_init(&virt_audio->base, virtio_uses_msix())) {
		free(virt_audio);
		return -1;
	}
	virtio_set_io_bar(&virt_audio->base, 0);

	return 0;
}

static void
virtio_audio_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_audio *virt_audio;

	virt_audio = dev->arg;
	if (!virt_audio) {
		DPRINTF(("%s: virtio_audio is NULL!\n", __func__));
		return;
	}
	if (virt_audio->vbs_k.kstatus == VIRTIO_DEV_STARTED) {
		DPRINTF(("%s: deinit virtio_audio_k!\n", __func__));
		virtio_audio_kernel_stop(virt_audio);
		virtio_audio_kernel_reset(virt_audio);
		virt_audio->vbs_k.kstatus = VIRTIO_DEV_INITIAL;
		if (virt_audio->vbs_k.audio_fd < 0) {
			WPRINTF(("virtio_audio: %s  doesn't open!\n",
				vbs_k_audio_dev_path));
			return;
		}
		close(virt_audio->vbs_k.audio_fd);
		virt_audio->vbs_k.audio_fd = -1;
	}
	pthread_mutex_destroy(&virt_audio->mtx);
	DPRINTF(("%s: free struct virtio_audio!\n", __func__));
	free((struct virtio_audio *)dev->arg);
}

struct pci_vdev_ops pci_ops_virtio_audio = {
	.class_name	= "virtio-audio",
	.vdev_init	= virtio_audio_init,
	.vdev_deinit	= virtio_audio_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read
};

DEFINE_PCI_DEVTYPE(pci_ops_virtio_audio);
