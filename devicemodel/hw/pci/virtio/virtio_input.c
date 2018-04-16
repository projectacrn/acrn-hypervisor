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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <err.h>
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
#define VIRTIO_INPUT_S_HOSTCAPS		(VIRTIO_F_VERSION_1)

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
	VIRTIO_INPUT_S_HOSTCAPS,	/* our capabilities */
};

static void
virtio_input_reset(void *vdev)
{
	/* to be implemented */
}

static void
virtio_input_neg_features(void *vdev, uint64_t negotiated_features)
{
	/* to be implemented */
}

static void
virtio_input_set_status(void *vdev, uint64_t status)
{
	/* to be implemented */
}

static int
virtio_input_cfgread(void *vdev, int offset, int size, uint32_t *retval)
{
	/* to be implemented */
	return 0;
}

static int
virtio_input_cfgwrite(void *vdev, int offset, int size, uint32_t val)
{
	/* to be implemented */
	return 0;
}

static int
virtio_input_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	/* to be implemented */
	DPRINTF(("%s\n", __func__));
	(void)virtio_input_ops;
	return 0;
}

static void
virtio_input_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	/* to be implemented */
}

struct pci_vdev_ops pci_ops_virtio_input = {
	.class_name	= "virtio-input",
	.vdev_init	= virtio_input_init,
	.vdev_deinit	= virtio_input_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_input);
