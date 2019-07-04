/*-
 * Copyright (c) 2014 Nahanni Systems Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * virtio entropy device emulation.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"
#include "virtio_kernel.h"
#include "vmmapi.h"			/* for vmctx */

#define VIRTIO_RND_RINGSZ	64

/*
 * Per-device struct
 */
struct virtio_rnd {
	/* VBS-U variables */
	struct virtio_base base;
	struct virtio_vq_info vq;
	pthread_mutex_t mtx;
	uint64_t cfg;
	int fd;
	int in_progress;
	pthread_t rx_tid;
	pthread_mutex_t	rx_mtx;
	pthread_cond_t rx_cond;
	/* VBS-K variables */
	struct {
		enum VBS_K_STATUS status;
		int fd;
		struct vbs_dev_info dev;
		struct vbs_vqs_info vqs;
	} vbs_k;
};

static int virtio_rnd_debug;
#define DPRINTF(params) do { if (virtio_rnd_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

/* VBS-K interface functions */
static int virtio_rnd_kernel_init(struct virtio_rnd *);	/* open VBS-K chardev */
static int virtio_rnd_kernel_start(struct virtio_rnd *);
static int virtio_rnd_kernel_stop(struct virtio_rnd *);
static int virtio_rnd_kernel_reset(struct virtio_rnd *);
static int virtio_rnd_kernel_dev_set(struct vbs_dev_info *kdev,
				     const char *name, int vmid, int nvq,
				     uint32_t feature, uint64_t pio_start,
				     uint64_t pio_len);
static int virtio_rnd_kernel_vq_set(struct vbs_vqs_info *kvqs, unsigned int nvq,
				    unsigned int idx, uint16_t qsize,
				    uint32_t pfn, uint16_t msix_idx,
				    uint64_t msix_addr, uint32_t msix_data);

/* VBS-U virtio_ops */
static void virtio_rnd_reset(void *);
static void virtio_rnd_notify(void *, struct virtio_vq_info *);
static struct virtio_ops virtio_rnd_ops = {
	"virtio_rnd",		/* our name */
	1,			/* we support 1 virtqueue */
	0,			/* config reg size */
	virtio_rnd_reset,	/* reset */
	virtio_rnd_notify,	/* device-wide qnotify */
	NULL,			/* read virtio config */
	NULL,			/* write virtio config */
	NULL,			/* apply negotiated features */
	NULL,			/* called on guest set status */
};

/* VBS-K virtio_ops */
static void virtio_rnd_k_no_notify(void *, struct virtio_vq_info *);
static void virtio_rnd_k_set_status(void *, uint64_t);
static struct virtio_ops virtio_rnd_ops_k = {
	"virtio_rnd",		/* our name */
	1,			/* we support 1 virtqueue */
	0,			/* config reg size */
	virtio_rnd_reset,	/* reset */
	virtio_rnd_k_no_notify,	/* device-wide qnotify */
	NULL,			/* read virtio config */
	NULL,			/* write virtio config */
	NULL,			/* apply negotiated features */
	virtio_rnd_k_set_status,/* called on guest set status */
};

/* VBS-K interface function implementations */
static void
virtio_rnd_k_no_notify(void *base, struct virtio_vq_info *vq)
{
	WPRINTF(("virtio_rnd: VBS-K mode! Should not reach here!!\n"));
}

/*
 * This callback gives us a chance to determine the timings
 * to kickoff VBS-K initialization
 */
static void
virtio_rnd_k_set_status(void *base, uint64_t status)
{
	struct virtio_rnd *rnd;
	int nvq;
	struct msix_table_entry *mte;
	uint64_t msix_addr = 0;
	uint32_t msix_data = 0;
	int rc, i, j;

	rnd = base;
	nvq = rnd->base.vops->nvq;

	if (rnd->vbs_k.status == VIRTIO_DEV_INIT_SUCCESS &&
	    (status & VIRTIO_CONFIG_S_DRIVER_OK)) {
		/* time to kickoff VBS-K side */
		/* init vdev first */
		rc = virtio_rnd_kernel_dev_set(&rnd->vbs_k.dev,
					       rnd->base.vops->name,
					       rnd->base.dev->vmctx->vmid,
					       nvq,
					       rnd->base.negotiated_caps,
					       /*
						* currently we let VBS-K handle
						* kick register
						*/
					       rnd->base.dev->bar[0].addr + 16,
					       2);

		for (i = 0; i < nvq; i++) {
			if (rnd->vq.msix_idx != VIRTIO_MSI_NO_VECTOR) {
				j = rnd->vq.msix_idx;
				mte = &rnd->base.dev->msix.table[j];
				msix_addr = mte->addr;
				msix_data = mte->msg_data;
			}
			rc = virtio_rnd_kernel_vq_set(&rnd->vbs_k.vqs,
						      nvq, i,
						      rnd->vq.qsize,
						      rnd->vq.pfn,
						      rnd->vq.msix_idx,
						      msix_addr,
						      msix_data);
			if (rc < 0) {
				WPRINTF(("rnd_kernel_set_vq fail,i %d ret %d\n",
					 i, rc));
				return;
			}
		}
		rc = virtio_rnd_kernel_start(rnd);
		if (rc < 0) {
			WPRINTF(("virtio_rnd_kernel_start() failed\n"));
			rnd->vbs_k.status = VIRTIO_DEV_START_FAILED;
		} else {
			rnd->vbs_k.status = VIRTIO_DEV_STARTED;
		}
	}
}

/*
 * Called in virtio_rnd_init(), where the initialization of the
 * PCIe device emulation is still on the way by device model.
 */
static int
virtio_rnd_kernel_init(struct virtio_rnd *rnd)
{

	rnd->vbs_k.fd = open("/dev/vbs_rng", O_RDWR);
	if (rnd->vbs_k.fd < 0) {
		WPRINTF(("Failed to open /dev/vbs_k_rng!\n"));
		return -VIRTIO_ERROR_FD_OPEN_FAILED;
	}
	DPRINTF(("Open /dev/vbs_rng success!\n"));

	memset(&rnd->vbs_k.dev, 0, sizeof(struct vbs_dev_info));
	memset(&rnd->vbs_k.vqs, 0, sizeof(struct vbs_vqs_info));

	return VIRTIO_SUCCESS;
}

static int
virtio_rnd_kernel_dev_set(struct vbs_dev_info *kdev, const char *name,
			  int vmid, int nvq, uint32_t feature,
			  uint64_t pio_start, uint64_t pio_len)
{
	/* FE driver has set VIRTIO_CONFIG_S_DRIVER_OK */

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
virtio_rnd_kernel_vq_set(struct vbs_vqs_info *kvqs, unsigned int nvq,
			 unsigned int idx, uint16_t qsize, uint32_t pfn,
			 uint16_t msix_idx, uint64_t msix_addr,
			 uint32_t msix_data)
{
	/* FE driver has set VIRTIO_CONFIG_S_DRIVER_OK */
	if (nvq <= idx) {
		WPRINTF(("%s: wrong idx!\n", __func__));
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
virtio_rnd_kernel_start(struct virtio_rnd *rnd)
{
	if (vbs_kernel_start(rnd->vbs_k.fd,
			     &rnd->vbs_k.dev,
			     &rnd->vbs_k.vqs) < 0) {
		WPRINTF(("Failed in vbs_k_start!\n"));
		return -VIRTIO_ERROR_START;
	}

	DPRINTF(("vbs_k_started!\n"));
	return VIRTIO_SUCCESS;
}

static int
virtio_rnd_kernel_stop(struct virtio_rnd *rnd)
{
	/* device specific cleanups here */
	return vbs_kernel_stop(rnd->vbs_k.fd);
}

static int
virtio_rnd_kernel_reset(struct virtio_rnd *rnd)
{
	memset(&rnd->vbs_k.dev, 0, sizeof(struct vbs_dev_info));
	memset(&rnd->vbs_k.vqs, 0, sizeof(struct vbs_vqs_info));

	return vbs_kernel_reset(rnd->vbs_k.fd);
}

static void
virtio_rnd_reset(void *base)
{
	struct virtio_rnd *rnd;

	rnd = base;

	DPRINTF(("virtio_rnd: device reset requested !\n"));
	virtio_reset_dev(&rnd->base);
	DPRINTF(("virtio_rnd: kstatus %d\n", rnd->vbs_k.status));
	if (rnd->vbs_k.status == VIRTIO_DEV_STARTED) {
		DPRINTF(("virtio_rnd: VBS-K reset requested!\n"));
		virtio_rnd_kernel_stop(rnd);
		virtio_rnd_kernel_reset(rnd);
		rnd->vbs_k.status = VIRTIO_DEV_INIT_SUCCESS;
	}
}

static void *
virtio_rnd_get_entropy(void *param)
{
	struct virtio_rnd *rnd = param;
	struct virtio_vq_info *vq = &rnd->vq;
	struct iovec iov;
	uint16_t idx;
	ssize_t len;

	for (;;) {
		pthread_mutex_lock(&rnd->rx_mtx);
		rnd->in_progress = 0;

		/*
		 * Checking the avail ring here serves two purposes:
		 *  - avoid vring processing due to spurious wakeups
		 *  - catch missing notifications before acquiring rx_mtx
		 */
		while (!vq_has_descs(vq))
			pthread_cond_wait(&rnd->rx_cond, &rnd->rx_mtx);

		rnd->in_progress = 1;
		pthread_mutex_unlock(&rnd->rx_mtx);

		do {
			vq_getchain(vq, &idx, &iov, 1, NULL);
			len = read(rnd->fd, iov.iov_base, iov.iov_len);
			if (len <= 0) {
				vq_retchain(vq);
				vq_endchains(vq, 0);

				/* no data available */
				if (len == -1 && errno == EAGAIN)
					return NULL;
				break;
			}

			/* release this chain and handle more */
			vq_relchain(vq, idx, len);
		} while (vq_has_descs(vq));

		/* at least one avail ring element has been processed */
		vq_endchains(vq, 1);
	}
}

static void
virtio_rnd_notify(void *base, struct virtio_vq_info *vq)
{
	struct virtio_rnd *rnd = base;

	/* Any ring entries to process */
	if (!vq_has_descs(vq))
		return;

	/* Signal the thread for processing */
	pthread_mutex_lock(&rnd->rx_mtx);
	if (rnd->in_progress == 0)
		pthread_cond_signal(&rnd->rx_cond);
	pthread_mutex_unlock(&rnd->rx_mtx);
}

static int
virtio_rnd_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_rnd *rnd = NULL;
	int fd;
	pthread_mutexattr_t attr;
	int rc;
	char *opt;
	char *vbs_k_opt = NULL;
	enum VBS_K_STATUS kstat = VIRTIO_DEV_INITIAL;
	char tname[MAXCOMLEN + 1];

	while ((opt = strsep(&opts, ",")) != NULL) {
		/* vbs_k_opt should be kernel=on */
		vbs_k_opt = strsep(&opt, "=");
		DPRINTF(("vbs_k_opt is %s\n", vbs_k_opt));
		if (opt != NULL) {
			if (strncmp(opt, "on", 2) == 0)
				kstat = VIRTIO_DEV_PRE_INIT;
			WPRINTF(("virtio_rnd: VBS-K initializing..."));
		}
	}

	/*
	 * Should always be able to open /dev/random.
	 */
	fd = open("/dev/random", O_RDONLY);
	if (fd < 0) {
		WPRINTF(("virtio_rnd: open failed: /dev/random \n"));
		return -1;
	}

	rnd = calloc(1, sizeof(struct virtio_rnd));
	if (!rnd) {
		WPRINTF(("virtio_rnd: calloc returns NULL\n"));
		goto fail;
	}

	rnd->vbs_k.status = kstat;

	/* init mutex attribute properly */
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		DPRINTF(("mutexattr init failed with erro %d!\n", rc));
	if (virtio_uses_msix()) {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
		if (rc)
			DPRINTF(("virtio_msix: mutexattr_settype failed with "
				"error %d!\n", rc));
	} else {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		if (rc)
			DPRINTF(("virtio_intx: mutexattr_settype failed with "
				"error %d!\n", rc));
	}
	rc = pthread_mutex_init(&rnd->mtx, &attr);
	if (rc)
		DPRINTF(("mutex init failed with error %d!\n", rc));

	if (rnd->vbs_k.status == VIRTIO_DEV_PRE_INIT) {
		DPRINTF(("%s: VBS-K option detected!\n", __func__));
		virtio_linkup(&rnd->base, &virtio_rnd_ops_k,
			      rnd, dev, &rnd->vq, BACKEND_VBSK);
		rc = virtio_rnd_kernel_init(rnd);
		if (rc < 0) {
			WPRINTF(("virtio_rnd: VBS-K init failed,error %d!\n",
				 rc));
			rnd->vbs_k.status = VIRTIO_DEV_INIT_FAILED;
		} else {
			rnd->vbs_k.status = VIRTIO_DEV_INIT_SUCCESS;
		}
	}
	if (rnd->vbs_k.status == VIRTIO_DEV_INITIAL ||
	    rnd->vbs_k.status != VIRTIO_DEV_INIT_SUCCESS) {
		DPRINTF(("%s: fallback to VBS-U...\n", __func__));
		virtio_linkup(&rnd->base, &virtio_rnd_ops, rnd, dev, &rnd->vq, BACKEND_VBSU);
	}

	rnd->base.mtx = &rnd->mtx;

	rnd->vq.qsize = VIRTIO_RND_RINGSZ;

	/* keep /dev/random opened while emulating */
	rnd->fd = fd;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_RANDOM);
	pci_set_cfgdata16(dev, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_CRYPTO);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_ENTROPY);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	if (virtio_interrupt_init(&rnd->base, virtio_uses_msix())) {
		goto fail;
	}

	virtio_set_io_bar(&rnd->base, 0);

	rnd->in_progress = 0;
	pthread_mutex_init(&rnd->rx_mtx, NULL);
	pthread_cond_init(&rnd->rx_cond, NULL);
	pthread_create(&rnd->rx_tid, NULL, virtio_rnd_get_entropy,
		       (void *)rnd);
	snprintf(tname, sizeof(tname), "vtrnd-%d:%d tx", dev->slot,
		 dev->func);
	pthread_setname_np(rnd->rx_tid, tname);

	return 0;

fail:
	close(fd);
	if (rnd) {
		if (rnd->vbs_k.status == VIRTIO_DEV_INIT_SUCCESS) {
			/* VBS-K is in use */
			close(rnd->vbs_k.fd);
		}
		free(rnd);
	}
	return -1;
}

static void
virtio_rnd_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_rnd *rnd;
	void *jval;

	rnd = dev->arg;
	if (rnd == NULL) {
		DPRINTF(("%s: rnd is NULL\n", __func__));
		return;
	}

	pthread_cancel(rnd->rx_tid);
	pthread_join(rnd->rx_tid, &jval);

	if (rnd->vbs_k.status == VIRTIO_DEV_STARTED) {
		DPRINTF(("%s: deinit virtio_rnd_k!\n", __func__));
		virtio_rnd_kernel_stop(rnd);
		virtio_rnd_kernel_reset(rnd);
		rnd->vbs_k.status = VIRTIO_DEV_INITIAL;
		if (rnd->vbs_k.fd >= 0) {
			close(rnd->vbs_k.fd);
			rnd->vbs_k.fd = -1;
		}
	}

	if (rnd->fd >= 0) {
		close(rnd->fd);
		rnd->fd = -1;
	}
	DPRINTF(("%s: free struct virtio_rnd!\n", __func__));
	free(rnd);
}


struct pci_vdev_ops pci_ops_virtio_rnd = {
	.class_name	= "virtio-rnd",
	.vdev_init	= virtio_rnd_init,
	.vdev_deinit	= virtio_rnd_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_rnd);
