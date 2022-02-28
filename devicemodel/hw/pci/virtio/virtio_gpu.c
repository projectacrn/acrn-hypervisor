/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * virtio-gpu device
 *
 */
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"
#include "vdisplay.h"

/*
 * Queue definitions.
 */
#define VIRTIO_GPU_CONTROLQ	0
#define VIRTIO_GPU_CURSORQ	1
#define VIRTIO_GPU_QNUM		2

/*
 * Virtqueue size.
 */
#define VIRTIO_GPU_RINGSZ	64
#define VIRTIO_GPU_MAXSEGS	256

/*
 * Feature bits
 */
#define VIRTIO_GPU_F_EDID		1
#define VIRTIO_GPU_F_RESOURCE_UUID	2
#define VIRTIO_GPU_F_RESOURCE_BLOB	3
#define VIRTIO_GPU_F_CONTEXT_INIT	4

/*
 * Host capabilities
 */
#define VIRTIO_GPU_S_HOSTCAPS	(1UL << VIRTIO_F_VERSION_1) | \
				(1UL << VIRTIO_GPU_F_EDID)

/*
 * Device events
 */
#define VIRTIO_GPU_EVENT_DISPLAY	(1 << 0)

/*
 * Generic definitions
 */
#define VIRTIO_GPU_MAX_SCANOUTS 16
#define VIRTIO_GPU_FLAG_FENCE (1 << 0)
#define VIRTIO_GPU_FLAG_INFO_RING_IDX (1 << 1)

/*
 * Config space "registers"
 */
struct virtio_gpu_config {
	uint32_t events_read;
	uint32_t events_clear;
	uint32_t num_scanouts;
	uint32_t num_capsets;
};

enum virtio_gpu_ctrl_type {
	/* 2d commands */
	VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
	VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
	VIRTIO_GPU_CMD_RESOURCE_UNREF,
	VIRTIO_GPU_CMD_SET_SCANOUT,
	VIRTIO_GPU_CMD_RESOURCE_FLUSH,
	VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
	VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
	VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
	VIRTIO_GPU_CMD_GET_CAPSET_INFO,
	VIRTIO_GPU_CMD_GET_CAPSET,
	VIRTIO_GPU_CMD_GET_EDID,
	VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID,
	VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB,
	VIRTIO_GPU_CMD_SET_SCANOUT_BLOB,

	/* cursor commands */
	VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
	VIRTIO_GPU_CMD_MOVE_CURSOR,

	/* success responses */
	VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
	VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
	VIRTIO_GPU_RESP_OK_CAPSET_INFO,
	VIRTIO_GPU_RESP_OK_CAPSET,
	VIRTIO_GPU_RESP_OK_EDID,
	VIRTIO_GPU_RESP_OK_RESOURCE_UUID,
	VIRTIO_GPU_RESP_OK_MAP_INFO,

	/* error responses */
	VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
	VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
	VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
	VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
	VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
	VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
};

struct virtio_gpu_ctrl_hdr {
	uint32_t type;
	uint32_t flags;
	uint64_t fence_id;
	uint32_t ctx_id;
	uint8_t ring_idx;
	uint8_t padding[3];
};

/*
 * Command: VIRTIO_GPU_CMD_GET_EDID
 */
struct virtio_gpu_get_edid {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t scanout;
	uint32_t padding;
};

struct virtio_gpu_resp_edid {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t size;
	uint32_t padding;
	uint8_t edid[1024];
};

/*
 * Per-device struct
 */
struct virtio_gpu {
	struct virtio_base base;
	struct virtio_vq_info vq[VIRTIO_GPU_QNUM];
	struct virtio_gpu_config cfg;
	pthread_mutex_t	mtx;
	int vdpy_handle;
};

struct virtio_gpu_command {
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu *gpu;
	struct virtio_vq_info *vq;
	struct iovec *iov;
	uint32_t iovcnt;
	bool finished;
	uint32_t iolen;
};

static void virtio_gpu_reset(void *vdev);
static int virtio_gpu_cfgread(void *, int, int, uint32_t *);
static int virtio_gpu_cfgwrite(void *, int, int, uint32_t);
static void virtio_gpu_neg_features(void *, uint64_t);
static void virtio_gpu_set_status(void *, uint64_t);

static struct virtio_ops virtio_gpu_ops = {
	"virtio-gpu",			/* our name */
	VIRTIO_GPU_QNUM,		/* we currently support 2 virtqueues */
	sizeof(struct virtio_gpu_config),/* config reg size */
	virtio_gpu_reset,		/* reset */
	NULL,				/* device-wide qnotify */
	virtio_gpu_cfgread,		/* read PCI config */
	virtio_gpu_cfgwrite,		/* write PCI config */
	virtio_gpu_neg_features,	/* apply negotiated features */
	virtio_gpu_set_status,		/* called on guest set status */
};
static int virtio_gpu_device_cnt = 0;

static void
virtio_gpu_set_status(void *vdev, uint64_t status)
{
	struct virtio_gpu *gpu;

	pr_dbg("virtio-gpu setting deivce status 0x%x.\n", status);
	gpu = vdev;
	gpu->base.status = status;
}

static void
virtio_gpu_reset(void *vdev)
{
	struct virtio_gpu *gpu;

	pr_dbg("Resetting virtio-gpu device.\n");
	gpu = vdev;
	virtio_reset_dev(&gpu->base);
}

static int
virtio_gpu_cfgread(void *vdev, int offset, int size, uint32_t *retval)
{
	struct virtio_gpu *gpu;
	void *ptr;

	gpu = vdev;
	ptr = (uint8_t *)&gpu->cfg + offset;
	memcpy(retval, ptr, size);

	return 0;
}

static int
virtio_gpu_cfgwrite(void *vdev, int offset, int size, uint32_t value)
{
	struct virtio_gpu *gpu;
	void *ptr;

	gpu = vdev;
	ptr = (uint8_t *)&gpu->cfg + offset;
	if (offset == offsetof(struct virtio_gpu_config, events_clear)) {

		memcpy(ptr, &value, size);
		gpu->cfg.events_read &= ~value;
		gpu->cfg.events_clear &= ~value;
	}
	pr_err("%s: write to read-only regisiters.\n", __func__);

	return 0;
}

static void
virtio_gpu_neg_features(void *vdev, uint64_t negotiated_features)
{
	struct virtio_gpu *gpu;

	pr_dbg("virtio-gpu driver negotiated feature bits 0x%x.\n",
			negotiated_features);
	gpu = vdev;
	gpu->base.negotiated_caps = negotiated_features;
}

static void
virtio_gpu_update_resp_fence(struct virtio_gpu_ctrl_hdr *hdr,
		struct virtio_gpu_ctrl_hdr *resp)
{
	if ((hdr == NULL ) || (resp == NULL))
		return;

	if(hdr->flags & VIRTIO_GPU_FLAG_FENCE) {
		resp->flags |= VIRTIO_GPU_FLAG_FENCE;
		resp->fence_id = hdr->fence_id;
	}
}

static void
virtio_gpu_cmd_unspec(struct virtio_gpu_command *cmd)
{
	struct virtio_gpu_ctrl_hdr resp;

	pr_info("virtio-gpu: unspec commands received.\n");
	memset(&resp, 0, sizeof(resp));
	cmd->iolen = sizeof(resp);
	resp.type = VIRTIO_GPU_RESP_ERR_UNSPEC;
	virtio_gpu_update_resp_fence(&cmd->hdr, &resp);
	memcpy(cmd->iov[cmd->iovcnt - 1].iov_base, &resp, sizeof(resp));
}

static void
virtio_gpu_update_resp_fence(struct virtio_gpu_ctrl_hdr *hdr,
			struct virtio_gpu_ctrl_hdr *resp)
{
	if ((hdr == NULL ) || (resp == NULL))
		return;

	if(hdr->flags & VIRTIO_GPU_FLAG_FENCE) {
		resp->flags |= VIRTIO_GPU_FLAG_FENCE;
		resp->fence_id = hdr->fence_id;
	}
}

static void
virtio_gpu_cmd_get_edid(struct virtio_gpu_command *cmd)
{
	struct virtio_gpu_get_edid req;
	struct virtio_gpu_resp_edid resp;
	struct virtio_gpu *gpu;

	gpu = cmd->gpu;
	memcpy(&req, cmd->iov[0].iov_base, sizeof(req));
	cmd->iolen = sizeof(resp);
	memset(&resp, 0, sizeof(resp));
	/* Only one EDID block is enough */
	resp.size = 128;
	virtio_gpu_update_resp_fence(&cmd->hdr, &resp.hdr);
	vdpy_get_edid(gpu->vdpy_handle, resp.edid, resp.size);
	memcpy(cmd->iov[1].iov_base, &resp, sizeof(resp));
}

static void
virtio_gpu_ctrl_bh(void *data)
{
	struct virtio_gpu *vdev;
	struct virtio_vq_info *vq;
	struct virtio_gpu_command cmd;
	struct iovec iov[VIRTIO_GPU_MAXSEGS];
	uint16_t flags[VIRTIO_GPU_MAXSEGS];
	int n;
	uint16_t idx;

	vq = (struct virtio_vq_info *)data;
	vdev = (struct virtio_gpu *)(vq->base);
	cmd.gpu = vdev;
	cmd.iolen = 0;

	while (vq_has_descs(vq)) {
		n = vq_getchain(vq, &idx, iov, VIRTIO_GPU_MAXSEGS, flags);
		if (n < 0) {
			pr_err("virtio-gpu: invalid descriptors\n");
			return;
		}
		if (n == 0) {
			pr_err("virtio-gpu: get no available descriptors\n");
			return;
		}

		cmd.iovcnt = n;
		cmd.iov = iov;
		memcpy(&cmd.hdr, iov[0].iov_base,
			sizeof(struct virtio_gpu_ctrl_hdr));

		switch (cmd.hdr.type) {
		case VIRTIO_GPU_CMD_GET_EDID:
			virtio_gpu_cmd_get_edid(&cmd);
		default:
			virtio_gpu_cmd_unspec(&cmd);
			break;
		}

		vq_relchain(vq, idx, cmd.iolen); /* Release the chain */
	}
	vq_endchains(vq, 1);	/* Generate interrupt if appropriate. */
}

static void
virtio_gpu_notify_controlq(void *vdev, struct virtio_vq_info *vq)
{
	virtio_gpu_ctrl_bh(vq);
}

static void
virtio_gpu_notify_cursorq(void *vdev, struct virtio_vq_info *vq)
{
	struct virtio_gpu_command cmd;
	struct virtio_gpu_ctrl_hdr hdr;
	struct iovec iov[VIRTIO_GPU_MAXSEGS];
	int n;
	uint16_t idx;

	cmd.gpu = vdev;
	cmd.iolen = 0;
	while (vq_has_descs(vq)) {
		n = vq_getchain(vq, &idx, iov, VIRTIO_GPU_MAXSEGS, NULL);
		if (n < 0) {
			pr_err("virtio-gpu: invalid descriptors\n");
			return;
		}
		if (n == 0) {
			pr_err("virtio-gpu: get no available descriptors\n");
			return;
		}
		cmd.iovcnt = n;
		cmd.iov = iov;
		memcpy(&hdr, iov[0].iov_base, sizeof(hdr));
		switch (hdr.type) {
		default:
			virtio_gpu_cmd_unspec(&cmd);
			break;
		}

		vq_relchain(vq, idx, cmd.iolen); /* Release the chain */
	}
	vq_endchains(vq, 1);	/* Generate interrupt if appropriate. */
}

static int
virtio_gpu_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_gpu *gpu;
	pthread_mutexattr_t attr;
	int rc = 0;

	if (virtio_gpu_device_cnt) {
		pr_err("%s: only 1 virtio-gpu device can be created.\n", __func__);
		return -1;
	}
	virtio_gpu_device_cnt++;

	/* allocate the virtio-gpu device */
	gpu = calloc(1, sizeof(struct virtio_gpu));
	if (!gpu) {
		pr_err("%s: out of memory\n", __func__);
		return -1;
	}

	/* init mutex attribute properly to avoid deadlock */
	rc = pthread_mutexattr_init(&attr);
	if (rc) {
		pr_err("%s: mutexattr init failed with erro %d!\n",
			       __func__, rc);
		return rc;
	}
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc) {
		pr_err("%s: mutexattr_settype failed with error %d!\n",
			       __func__, rc);
		return rc;
	}
	rc = pthread_mutex_init(&gpu->mtx, &attr);
	if (rc) {
		pr_err("%s: pthread_mutex_init failed with error %d!\n",
			       __func__,rc);
		return rc;
	}

	/* register the virtio_gpu_ops to virtio framework */
	virtio_linkup(&gpu->base,
			&virtio_gpu_ops,
			gpu,
			dev,
			gpu->vq,
			BACKEND_VBSU);

	gpu->base.mtx = &gpu->mtx;
	gpu->base.device_caps = VIRTIO_GPU_S_HOSTCAPS;

	/* set queue size */
	gpu->vq[VIRTIO_GPU_CONTROLQ].qsize = VIRTIO_GPU_RINGSZ;
	gpu->vq[VIRTIO_GPU_CONTROLQ].notify = virtio_gpu_notify_controlq;
	gpu->vq[VIRTIO_GPU_CURSORQ].qsize = VIRTIO_GPU_RINGSZ;
	gpu->vq[VIRTIO_GPU_CURSORQ].notify = virtio_gpu_notify_cursorq;

	/* prepare the config space */
	gpu->cfg.events_read = 0;
	gpu->cfg.events_clear = 0;
	gpu->cfg.num_scanouts = 1;
	gpu->cfg.num_capsets = 0;

	/* config the device id and vendor id according to spec */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_GPU);
	pci_set_cfgdata16(dev, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata16(dev, PCIR_REVID, 1);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_DISPLAY);
	pci_set_cfgdata8(dev, PCIR_SUBCLASS, PCIS_DISPLAY_OTHER);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_GPU);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	/*** PCI Config BARs setup ***/
	rc = virtio_interrupt_init(&gpu->base, virtio_uses_msix());
	if (rc) {
		pr_err("%s, interrupt_init failed.\n", __func__);
		return rc;
	}
	rc = virtio_set_modern_bar(&gpu->base, true);
	if (rc) {
		pr_err("%s, set modern bar failed.\n", __func__);
		return rc;
	}
	gpu->vdpy_handle = vdpy_init();

	return 0;
}

static void
virtio_gpu_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_gpu *gpu;

	gpu = (struct virtio_gpu *)dev->arg;
	if (gpu) {
		pthread_mutex_destroy(&gpu->mtx);
		free(gpu);
	}
	virtio_gpu_device_cnt--;
	vdpy_deinit(gpu->vdpy_handle);
}

static void
virtio_gpu_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		int baridx, uint64_t offset, int size, uint64_t value)
{
	virtio_pci_write(ctx, vcpu, dev, baridx, offset, size, value);
}

static uint64_t
virtio_gpu_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		int baridx, uint64_t offset, int size)
{
	return virtio_pci_read(ctx, vcpu, dev, baridx, offset, size);
}


struct pci_vdev_ops pci_ops_virtio_gpu = {
	.class_name     = "virtio-gpu",
	.vdev_init      = virtio_gpu_init,
	.vdev_deinit    = virtio_gpu_deinit,
	.vdev_barwrite	= virtio_gpu_write,
	.vdev_barread	= virtio_gpu_read
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_gpu);
