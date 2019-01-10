/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sys/ioctl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <linux/types.h>
#include <linux/gpio.h>

#include "acpi.h"
#include "dm.h"
#include "pci_core.h"
#include "mevent.h"
#include "virtio.h"

/*
 *  GPIO virtualization architecture
 *
 *                +--------------------------+
 *                |ACRN DM                   |
 *                |  +--------------------+  |
 *                |  |                    |  |  virtqueue
 *                |  |   GPIO mediator    |<-+-----------+
 *                |  |                    |  |           |
 *                |  +-+-----+--------+---+  |           |
 *   User space   +----|-----|--------|------+           |
 *           +---------+     |        |                  |
 *           v               v        v                  |
 *   +----------------+   +-----+   +----------------+   | +---------------+
 *  -+ /dev/gpiochip0 +---+ ... +---+ /dev/gpiochipN +-----+ UOS           +-
 *   +                +   +     +   +                +   | +/dev/gpiochip0 +
 *   +------------+---+   +--+--+   +-------------+--+   | +------+--------+
 *   Kernel space |          +--------------+     |      |        |
 *                +--------------------+    |     |      |        |
 *                                     v    v     v      |        v
 *   +---------------------+    +---------------------+  |  +-------------+
 *   |                     |    |                     |  |  |UOS Virtio   |
 *   |  pinctrl subsystem  |<---+  gpiolib subsystem  |  +->+GPIO Driver  |
 *   |                     |    |                     |     |             |
 *   +--------+------------+    +----------+----------+     +-------------+
 *            |   +------------------------+
 *            |   |
 *  ----------|---|----------------------------------------------------------
 *   Hardware |   |
 *            v   v
 *   +------------------+
 *   |                  |
 *   | GPIO controllers |
 *   |                  |
 *   +------------------+
 */

static int gpio_debug;
static FILE *dbg_file;
#define VIRTIO_GPIO_LOG_INIT do {					\
	if (gpio_debug && !dbg_file) {					\
		dbg_file = fopen("/tmp/log.gpio", "w+");		\
		if (!dbg_file)						\
			printf("virtio_gpio log open failed\r\n");	\
	}								\
} while (0)

#define VIRTIO_GPIO_LOG_DEINIT	do {					\
	if (dbg_file) {							\
		fclose(dbg_file);					\
		dbg_file = NULL;					\
	}								\
} while (0)

#define DPRINTF(format, arg...) do {					\
	if (gpio_debug && dbg_file) {					\
		fprintf(dbg_file, format, arg);				\
		fflush(dbg_file);					\
	}								\
} while (0)

/* Virtio GPIO supports maximum number of virtual gpio */
#define VIRTIO_GPIO_MAX_VLINES	64

/* Virtio GPIO supports maximum native gpio chips */
#define VIRTIO_GPIO_MAX_CHIPS	8

/* Virtio GPIO virtqueue numbers*/
#define VIRTIO_GPIO_MAXQ	1

/* Virtio GPIO capabilities */
#define VIRTIO_GPIO_F_CHIP	1
#define VIRTIO_GPIO_S_HOSTCAPS	VIRTIO_GPIO_F_CHIP

/* make virtio gpio mediator a singleton mode */
static bool virtio_gpio_is_active;

struct virtio_gpio_config {
	uint16_t	base;	/* base number */
	uint16_t	ngpio;	/* number of gpios */
} __attribute__((packed));

struct virtio_gpio {
	pthread_mutex_t	mtx;
	struct virtio_base	base;
	struct virtio_vq_info	queues[VIRTIO_GPIO_MAXQ];
	struct virtio_gpio_config	config;
};

static void virtio_gpio_reset(void *vdev)
{
	struct virtio_gpio *gpio;

	gpio = vdev;

	DPRINTF("%s", "virtio_gpio: device reset requested!\n");
	virtio_reset_dev(&gpio->base);
}

static int
virtio_gpio_cfgread(void *vdev, int offset, int size, uint32_t *retval)
{
	struct virtio_gpio *gpio = vdev;
	void *ptr;

	ptr = (uint8_t *)&gpio->config + offset;
	memcpy(retval, ptr, size);
	return 0;
}

static struct virtio_ops virtio_gpio_ops = {
	"virtio_gpio",			/* our name */
	VIRTIO_GPIO_MAXQ,		/* we support VTGPIO_MAXQ virtqueues */
	sizeof(struct virtio_gpio_config), /* config reg size */
	virtio_gpio_reset,		/* reset */
	NULL,				/* device-wide qnotify */
	virtio_gpio_cfgread,		/* read virtio config */
	NULL,				/* write virtio config */
	NULL,				/* apply negotiated features */
	NULL,				/* called on guest set status */

};

static void
virtio_gpio_proc(struct virtio_gpio *gpio, struct iovec *iov, uint16_t flag)
{
	/* Implemented in the subsequent patch */
}

static void
virtio_gpio_notify(void *vdev, struct virtio_vq_info *vq)
{
	struct iovec iov[2];
	struct virtio_gpio *gpio;
	uint16_t idx;
	int n;

	gpio = (struct virtio_gpio *)vdev;
	if (vq_has_descs(vq)) {
		n = vq_getchain(vq, &idx, iov, 2, NULL);
		assert(n < 3);

		virtio_gpio_proc(gpio, iov, n);
		/*
		 * Release this chain and handle more
		 */
		vq_relchain(vq, idx, 1);

	}
}

static int
virtio_gpio_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_gpio *gpio;
	pthread_mutexattr_t attr;
	int rc;

	/* Just support one bdf */
	if (virtio_gpio_is_active)
		return -1;

	VIRTIO_GPIO_LOG_INIT;

	if (!opts) {
		DPRINTF("%s", "virtio gpio: needs gpio information\n");
		rc = -EINVAL;
		goto init_fail;
	}

	gpio = calloc(1, sizeof(struct virtio_gpio));
	if (!gpio) {
		DPRINTF("%s", "virtio gpio: failed to calloc virtio_gpio\n");
		rc = -ENOMEM;
		goto init_fail;
	}

	/* init mutex attribute properly to avoid deadlock */
	rc = pthread_mutexattr_init(&attr);
	if (rc) {
		DPRINTF("mutexattr init failed with error %d!\n", rc);
		goto mtx_fail;
	}
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc) {
		DPRINTF("mutexattr_settype failed with error %d!\n", rc);
		goto fail;
	}

	rc = pthread_mutex_init(&gpio->mtx, &attr);
	if (rc) {
		DPRINTF("pthread_mutex_init failed with error %d!\n", rc);
		goto fail;
	}

	virtio_linkup(&gpio->base, &virtio_gpio_ops, gpio, dev, gpio->queues,
			BACKEND_VBSU);

	/* gpio base for frontend gpio chip */
	gpio->config.base = 0;

	gpio->base.device_caps = VIRTIO_GPIO_S_HOSTCAPS;
	gpio->base.mtx = &gpio->mtx;
	gpio->queues[0].qsize = 64;
	gpio->queues[0].notify = virtio_gpio_notify;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_GPIO);
	pci_set_cfgdata16(dev, PCIR_VENDOR, INTEL_VENDOR_ID);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_BASEPERIPH);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_GPIO);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, INTEL_VENDOR_ID);

	/* use BAR 1 to map MSI-X table and PBA, if we're using MSI-X */
	if (virtio_interrupt_init(&gpio->base, virtio_uses_msix())) {
		rc = -1;
		goto fail;
	}

	/* use BAR 0 to map config regs in IO space */
	virtio_set_io_bar(&gpio->base, 0);

	virtio_gpio_is_active = true;
	return 0;

fail:
	pthread_mutex_destroy(&gpio->mtx);

mtx_fail:
	free(gpio);
	dev->arg = NULL;

init_fail:
	VIRTIO_GPIO_LOG_DEINIT;
	return rc;
}

static void
virtio_gpio_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_gpio *gpio;

	DPRINTF("%s", "virtio gpio: pci_gpio_deinit\r\n");
	virtio_gpio_is_active = false;
	gpio = (struct virtio_gpio *)dev->arg;
	if (gpio) {
		pthread_mutex_destroy(&gpio->mtx);
		free(gpio);
		dev->arg = NULL;
	}

	VIRTIO_GPIO_LOG_DEINIT;
}

struct pci_vdev_ops pci_ops_virtio_gpio = {
	.class_name	= "virtio-gpio",
	.vdev_init	= virtio_gpio_init,
	.vdev_deinit	= virtio_gpio_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read,
};

DEFINE_PCI_DEVTYPE(pci_ops_virtio_gpio);
