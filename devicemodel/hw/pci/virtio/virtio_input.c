/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"
#include "mevent.h"
#include <linux/input.h>

static int virtio_input_debug;
#define DPRINTF(params) do { if (virtio_input_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

/*
 * Queue definitions.
 */
#define VIRTIO_INPUT_EVENT_QUEUE	0
#define VIRTIO_INPUT_STATUS_QUEUE	1
#define VIRTIO_INPUT_MAXQ		2

/*
 * Virtqueue size.
 */
#define VIRTIO_INPUT_RINGSZ		64

/*
 * Default size of the buffer used to hold events between SYN
 */
#define VIRTIO_INPUT_PACKET_SIZE	10

/*
 * Host capabilities
 */
#define VIRTIO_INPUT_S_HOSTCAPS		(ACRN_VIRTIO_F_VERSION_1)

enum virtio_input_config_select {
	VIRTIO_INPUT_CFG_UNSET		= 0x00,
	VIRTIO_INPUT_CFG_ID_NAME	= 0x01,
	VIRTIO_INPUT_CFG_ID_SERIAL	= 0x02,
	VIRTIO_INPUT_CFG_ID_DEVIDS	= 0x03,
	VIRTIO_INPUT_CFG_PROP_BITS	= 0x10,
	VIRTIO_INPUT_CFG_EV_BITS	= 0x11,
	VIRTIO_INPUT_CFG_ABS_INFO	= 0x12,
};

struct virtio_input_absinfo {
	uint32_t min;
	uint32_t max;
	uint32_t fuzz;
	uint32_t flat;
	uint32_t res;
};

struct virtio_input_devids {
	uint16_t bustype;
	uint16_t vendor;
	uint16_t product;
	uint16_t version;
};

struct virtio_input_event {
	uint16_t type;
	uint16_t code;
	uint32_t value;
};

/*
 * Device-specific configuration registers
 * To query a specific piece of configuration information FE driver sets
 * "select" and "subsel" accordingly, information size is returned in "size"
 * and information data is returned in union "u"
 */
struct virtio_input_config {
	uint8_t    select;
	uint8_t    subsel;
	uint8_t    size;
	uint8_t    reserved[5];
	union {
		char string[128];
		uint8_t bitmap[128];
		struct virtio_input_absinfo abs;
		struct virtio_input_devids ids;
	} u;
};

struct virtio_input_event_elem {
	struct virtio_input_event		event;
	struct iovec				iov;
	uint16_t				idx;
};

/*
 * Per-device struct
 */
struct virtio_input {
	struct virtio_base			base;
	struct virtio_vq_info			queues[VIRTIO_INPUT_MAXQ];
	pthread_mutex_t				mtx;
	struct mevent				*mevp;
	uint64_t				features;
	struct virtio_input_config		cfg;
	char					*evdev;
	char					*serial;
	int					fd;
	bool					ready;

	struct virtio_input_event_elem		*event_queue;
	uint32_t				event_qsize;
	uint32_t				event_qindex;
};

static void virtio_input_reset(void *);
static void virtio_input_neg_features(void *, uint64_t);
static void virtio_input_set_status(void *, uint64_t);
static int virtio_input_cfgread(void *, int, int, uint32_t *);
static int virtio_input_cfgwrite(void *, int, int, uint32_t);
static bool virtio_input_get_config(struct virtio_input *, uint8_t, uint8_t,
	struct virtio_input_config *);

static struct virtio_ops virtio_input_ops = {
	"virtio_input",			/* our name */
	VIRTIO_INPUT_MAXQ,		/* we support VTCON_MAXQ virtqueues */
	sizeof(struct virtio_input_config),	/* config reg size */
	virtio_input_reset,		/* reset */
	NULL,				/* device-wide qnotify */
	virtio_input_cfgread,		/* read virtio config */
	virtio_input_cfgwrite,		/* write virtio config */
	virtio_input_neg_features,	/* apply negotiated features */
	virtio_input_set_status,	/* called on guest set status */
};

static void
virtio_input_reset(void *vdev)
{
	struct virtio_input *vi;

	vi = vdev;

	DPRINTF(("vtinput: device reset requested!\n"));
	vi->ready = false;
	virtio_reset_dev(&vi->base);
}

static void
virtio_input_neg_features(void *vdev, uint64_t negotiated_features)
{
	struct virtio_input *vi = vdev;

	vi->features = negotiated_features;
}

static void
virtio_input_set_status(void *vdev, uint64_t status)
{
	struct virtio_input *vi = vdev;

	if (status & VIRTIO_CR_STATUS_DRIVER_OK) {
		if (!vi->ready)
			vi->ready = true;
	}
}

static int
virtio_input_cfgread(void *vdev, int offset, int size, uint32_t *retval)
{
	struct virtio_input *vi = vdev;
	struct virtio_input_config cfg;
	bool rc;

	rc = virtio_input_get_config(vi, vi->cfg.select,
		vi->cfg.subsel, &cfg);
	if (rc)
		memcpy(retval, (uint8_t *)&cfg + offset, size);
	else
		memset(retval, 0, size);

	return 0;
}

static int
virtio_input_cfgwrite(void *vdev, int offset, int size, uint32_t val)
{
	struct virtio_input *vi = vdev;

	if (offset == offsetof(struct virtio_input_config, select))
		vi->cfg.select = (uint8_t)val;
	else if (offset == offsetof(struct virtio_input_config, subsel))
		vi->cfg.subsel = (uint8_t)val;
	else
		DPRINTF(("vtinput: write to readonly reg %d\n", offset));

	return 0;
}

static bool
virtio_input_ignore_event(struct virtio_input_event *event)
{
	if (!event)
		return true;

	/*
	 * EV_MSC is configured as INPUT_PASS_TO_ALL. In the use case of
	 * virtio-input, there is a loop as follows:
	 * - A mt frame with (EV_MSC,*,*) is passed to FE.
	 * - FE will call virtinput_status to pass (EV_MSC,*,*) back to BE.
	 * - BE writes this event to evdev. Because (EV_MSC,*,*)
	 *   is configured as INPUT_PASS_TO_ALL, it will be written into
	 *   the event buffer of evdev then be read out by BE without
	 *   SYN followed.
	 * - Each mt frame will introduce one (EV_MSC,*,*).
	 *   Later the frame becomes larger and larger...
	 */
	if (event->type == EV_MSC)
		return true;
	return false;
}

static void
virtio_input_notify_event_vq(void *vdev, struct virtio_vq_info *vq)
{
	DPRINTF(("%s\n", __func__));
}

static void
virtio_input_notify_status_vq(void *vdev, struct virtio_vq_info *vq)
{
	struct virtio_input *vi;
	struct virtio_input_event event;
	struct input_event host_event;
	struct iovec iov;
	int n, len;
	uint16_t idx;

	vi = vdev;

	while (vq_has_descs(vq)) {
		n = vq_getchain(vq, &idx, &iov, 1, NULL);
		assert(n == 1);

		if (vi->fd > 0) {
			memcpy(&event, iov.iov_base, sizeof(event));
			if (!virtio_input_ignore_event(&event)) {
				host_event.type = event.type;
				host_event.code = event.code;
				host_event.value = event.value;
				if (gettimeofday(&host_event.time, NULL)) {
					WPRINTF(("vtinput: gettimeofday failed\n"));
					break;
				}
				len = write(vi->fd, &host_event, sizeof(host_event));
				if (len == -1)
					WPRINTF(("%s: write failed, len = %d, "
						"errno = %d\n",
						__func__, len, errno));
			}
		}
		vq_relchain(vq, idx, sizeof(event)); /* Release the chain */
	}
	vq_endchains(vq, 1);	/* Generate interrupt if appropriate. */
}

static void
virtio_input_send_event(struct virtio_input *vi,
			struct virtio_input_event *event)
{
	struct virtio_vq_info *vq;
	struct iovec iov;
	int n, i;
	uint16_t idx;

	if (!vi->ready)
		return;

	if (vi->event_qindex == vi->event_qsize) {
		vi->event_qsize++;
		vi->event_queue = realloc(vi->event_queue,
			vi->event_qsize *
			sizeof(struct virtio_input_event_elem));
		assert(vi->event_queue);
	}
	vi->event_queue[vi->event_qindex].event = *event;
	vi->event_qindex++;

	if (event->type != EV_SYN || event->code != SYN_REPORT)
		return;

	vq = &vi->queues[VIRTIO_INPUT_EVENT_QUEUE];
	for (i = 0; i < vi->event_qindex; i++) {
		if (!vq_has_descs(vq)) {
			while (i-- > 0)
				vq_retchain(vq);
			WPRINTF(("%s: not enough avail descs, dropped:%d\n",
				__func__, vi->event_qindex));
			goto out;
		}
		n = vq_getchain(vq, &idx, &iov, 1, NULL);
		assert(n == 1);
		vi->event_queue[i].iov = iov;
		vi->event_queue[i].idx = idx;
	}

	for (i = 0; i < vi->event_qindex; i++) {
		memcpy(vi->event_queue[i].iov.iov_base,
			&vi->event_queue[i].event,
			sizeof(struct virtio_input_event));
		vq_relchain(vq, vi->event_queue[i].idx,
			sizeof(struct virtio_input_event));
	}

out:
	vi->event_qindex = 0;
	vq_endchains(vq, 1);
}

static void
virtio_input_read_event(int fd __attribute__((unused)),
			enum ev_type t __attribute__((unused)),
			void *arg)
{
	struct virtio_input *vi = arg;
	struct virtio_input_event event;
	struct input_event host_event;
	int len;

	while (1) {
		len = read(vi->fd, &host_event, sizeof(host_event));
		if (len != sizeof(host_event)) {
			if (len == -1 && errno != EAGAIN)
				WPRINTF(("vtinput: host read failed! "
					"len = %d, errno = %d\n",
					len, errno));
			break;
		}

		event.type = host_event.type;
		event.code = host_event.code;
		event.value = host_event.value;
		virtio_input_send_event(vi, &event);
	}
}

static int
virtio_input_get_bitmap(struct virtio_input *vi, unsigned int cmd, int count,
			struct virtio_input_config *cfg)
{
	int i, size = -1;
	int rc;

	if (count <= 0)
		return -1;

	if (!cfg)
		return -1;

	memset(cfg, 0, sizeof(*cfg));
	rc = ioctl(vi->fd, cmd, cfg->u.bitmap);
	if (rc < 0)
		return -1;

	count = count / 8;
	for (i = count - 1; i >= 0; i--) {
		if (cfg->u.bitmap[i]) {
			size = i + 1;
			break;
		}
	}

	return size;
}

static bool
virtio_input_get_propbits(struct virtio_input *vi,
			  struct virtio_input_config *cfg)
{
	unsigned int cmd;
	int size;

	if (!cfg)
		return false;

	cmd = EVIOCGPROP(INPUT_PROP_CNT / 8);
	size = virtio_input_get_bitmap(vi, cmd, INPUT_PROP_CNT, cfg);
	if (size > 0) {
		cfg->select = VIRTIO_INPUT_CFG_PROP_BITS;
		cfg->subsel = 0;
		cfg->size = size;
		return true;
	}

	return false;
}

static bool
virtio_input_get_evbits(struct virtio_input *vi, int type,
			struct virtio_input_config *cfg)
{
	unsigned int cmd;
	int count, size;

	if (!cfg)
		return false;

	switch (type) {
	case EV_KEY:
		count = KEY_CNT;
		break;
	case EV_REL:
		count = REL_CNT;
		break;
	case EV_ABS:
		count = ABS_CNT;
		break;
	case EV_MSC:
		count = MSC_CNT;
		break;
	case EV_SW:
		count = SW_CNT;
		break;
	case EV_LED:
		count = LED_CNT;
		break;
	default:
		return false;
	}

	cmd = EVIOCGBIT(type, count / 8);
	size = virtio_input_get_bitmap(vi, cmd, count, cfg);
	if (size > 0) {
		cfg->select = VIRTIO_INPUT_CFG_EV_BITS;
		cfg->subsel = type;
		cfg->size = size;
		return true;
	}

	return false;
}

static bool
virtio_input_get_absinfo(struct virtio_input *vi, int axis,
			 struct virtio_input_config *cfg)
{
	struct virtio_input_config ev_cfg;
	struct input_absinfo abs;
	bool has_ev_abs;
	int rc;

	if (!cfg)
		return false;

	has_ev_abs = virtio_input_get_evbits(vi, EV_ABS, &ev_cfg);
	if (!has_ev_abs)
		return false;

	rc = ioctl(vi->fd, EVIOCGABS(axis), &abs);
	if (rc < 0)
		return false;

	cfg->u.abs.min = abs.minimum;
	cfg->u.abs.max = abs.maximum;
	cfg->u.abs.fuzz = abs.fuzz;
	cfg->u.abs.flat = abs.flat;
	cfg->u.abs.res = abs.resolution;

	cfg->select = VIRTIO_INPUT_CFG_ABS_INFO;
	cfg->subsel = axis;
	cfg->size = sizeof(struct virtio_input_absinfo);
	return true;
}

static bool
virtio_input_get_config(struct virtio_input *vi, uint8_t select,
			uint8_t subsel, struct virtio_input_config *cfg)
{
	struct input_id dev_ids;
	bool found = false;
	int rc;

	if (!cfg)
		return false;

	memset(cfg, 0, sizeof(*cfg));

	switch (select) {
	case VIRTIO_INPUT_CFG_ID_NAME:
		rc = ioctl(vi->fd, EVIOCGNAME(sizeof(cfg->u.string) - 1),
			cfg->u.string);
		if (rc >= 0) {
			cfg->select = VIRTIO_INPUT_CFG_ID_NAME;
			cfg->size = strnlen(cfg->u.string,
				sizeof(cfg->u.string));
			found = true;
		}
		break;
	case VIRTIO_INPUT_CFG_ID_SERIAL:
		if (vi->serial) {
			cfg->select = VIRTIO_INPUT_CFG_ID_SERIAL;
			cfg->size = snprintf(cfg->u.string,
				sizeof(cfg->u.string), "%s", vi->serial);
			found = true;
		}
		break;
	case VIRTIO_INPUT_CFG_ID_DEVIDS:
		rc = ioctl(vi->fd, EVIOCGID, &dev_ids);
		if (!rc) {
			cfg->u.ids.bustype = dev_ids.bustype;
			cfg->u.ids.vendor  = dev_ids.vendor;
			cfg->u.ids.product = dev_ids.product;
			cfg->u.ids.version = dev_ids.version;
			cfg->select = VIRTIO_INPUT_CFG_ID_DEVIDS;
			cfg->size = sizeof(struct virtio_input_devids);
			found = true;
		}
		break;
	case VIRTIO_INPUT_CFG_PROP_BITS:
		found = virtio_input_get_propbits(vi, cfg);
		break;
	case VIRTIO_INPUT_CFG_EV_BITS:
		found = virtio_input_get_evbits(vi, subsel, cfg);
		break;
	case VIRTIO_INPUT_CFG_ABS_INFO:
		found = virtio_input_get_absinfo(vi, subsel, cfg);
		break;
	default:
		break;
	}

	return found;
}

static int
virtio_input_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_input *vi;
	pthread_mutexattr_t attr;
	bool mutex_initialized = false;
	char *opt;
	int flags, ver;
	int rc;

	/* get evdev path from opts
	 * -s n,virtio-input,/dev/input/eventX[,serial]
	 */
	if (!opts) {
		WPRINTF(("%s: evdev path is NULL\n", __func__));
		return -1;
	}

	vi = calloc(1, sizeof(struct virtio_input));
	if (!vi) {
		WPRINTF(("%s: out of memory\n", __func__));
		return -1;
	}

	opt = strsep(&opts, ",");
	if (!opt) {
		WPRINTF(("%s: evdev path is NULL\n", __func__));
		goto fail;
	}

	vi->evdev = strdup(opt);
	if (!vi->evdev) {
		WPRINTF(("%s: strdup failed\n", __func__));
		goto fail;
	}

	if (opts) {
		vi->serial = strdup(opts);
		if (!vi->serial) {
			WPRINTF(("%s: strdup serial failed\n", __func__));
			goto fail;
		}
	}

	vi->fd = open(vi->evdev, O_RDWR);
	if (vi->fd < 0) {
		WPRINTF(("open %s failed %d\n", vi->evdev, errno));
		goto fail;
	}
	flags = fcntl(vi->fd, F_GETFL);
	fcntl(vi->fd, F_SETFL, flags | O_NONBLOCK);

	rc = ioctl(vi->fd, EVIOCGVERSION, &ver); /* is it a evdev device? */
	if (rc < 0) {
		WPRINTF(("%s: get version failed\n", vi->evdev));
		goto fail;
	}

	rc = ioctl(vi->fd, EVIOCGRAB, 1); /* exclusive access */
	if (rc < 0) {
		WPRINTF(("%s: grab device failed %d\n", vi->evdev, errno));
		goto fail;
	}

	/* init mutex attribute properly to avoid deadlock */
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		DPRINTF(("mutexattr init failed with erro %d!\n", rc));
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc)
		DPRINTF(("vtinput: mutexattr_settype failed with "
			"error %d!\n", rc));
	rc = pthread_mutex_init(&vi->mtx, &attr);
	if (rc)
		DPRINTF(("vtinput: pthread_mutex_init failed with "
			"error %d!\n", rc));
	mutex_initialized = (rc == 0) ? true : false;

	vi->event_qsize = VIRTIO_INPUT_PACKET_SIZE;
	vi->event_qindex = 0;
	vi->event_queue = calloc(vi->event_qsize,
		sizeof(struct virtio_input_event_elem));
	if (!vi->event_queue) {
		WPRINTF(("vtinput: could not alloc event queue buf\n"));
		goto fail;
	}

	vi->mevp = mevent_add(vi->fd, EVF_READ, virtio_input_read_event, vi, NULL, NULL);
	if (vi->mevp == NULL) {
		WPRINTF(("vtinput: could not register event\n"));
		goto fail;
	}

	virtio_linkup(&vi->base, &virtio_input_ops, vi, dev, vi->queues, BACKEND_VBSU);
	vi->base.mtx = &vi->mtx;
	vi->base.device_caps = VIRTIO_INPUT_S_HOSTCAPS;

	vi->queues[VIRTIO_INPUT_EVENT_QUEUE].qsize = VIRTIO_INPUT_RINGSZ;
	vi->queues[VIRTIO_INPUT_EVENT_QUEUE].notify =
		virtio_input_notify_event_vq;

	vi->queues[VIRTIO_INPUT_STATUS_QUEUE].qsize = VIRTIO_INPUT_RINGSZ;
	vi->queues[VIRTIO_INPUT_STATUS_QUEUE].notify =
		virtio_input_notify_status_vq;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, 0x1040 + VIRTIO_TYPE_INPUT);
	pci_set_cfgdata16(dev, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_INPUTDEV);
	pci_set_cfgdata8(dev, PCIR_SUBCLASS, PCIS_INPUTDEV_OTHER);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, 0x1040 + VIRTIO_TYPE_INPUT);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	if (virtio_interrupt_init(&vi->base, virtio_uses_msix())) {
		DPRINTF(("%s, interrupt_init failed!\n", __func__));
		goto fail;
	}
	rc = virtio_set_modern_bar(&vi->base, true);

	return rc;

fail:
	if (vi) {
		if (mutex_initialized)
			pthread_mutex_destroy(&vi->mtx);
		if (vi->event_queue) {
			free(vi->event_queue);
			vi->event_queue = NULL;
		}
		if (vi->mevp) {
			mevent_delete(vi->mevp);
			vi->mevp = NULL;
		}
		if (vi->fd > 0) {
			close(vi->fd);
			vi->fd = -1;
		}
		if (vi->serial) {
			free(vi->serial);
			vi->serial = NULL;
		}
		if (vi->evdev) {
			free(vi->evdev);
			vi->evdev = NULL;
		}
		free(vi);
		vi = NULL;
	}
	return -1;
}

static void
virtio_input_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_input *vi;

	vi = (struct virtio_input *)dev->arg;
	if (vi) {
		pthread_mutex_destroy(&vi->mtx);
		if (vi->event_queue)
			free(vi->event_queue);
		if (vi->mevp)
			mevent_delete(vi->mevp);
		if (vi->fd > 0)
			close(vi->fd);
		if (vi->evdev)
			free(vi->evdev);
		if (vi->serial)
			free(vi->serial);
		free(vi);
		vi = NULL;
	}
}

struct pci_vdev_ops pci_ops_virtio_input = {
	.class_name	= "virtio-input",
	.vdev_init	= virtio_input_init,
	.vdev_deinit	= virtio_input_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_input);
