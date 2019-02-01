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

struct gpio_line {
	char	name[32];		/* native gpio name */
	char	vname[32];		/* virtual gpio name */
	int	offset;			/* offset in real chip */
	int	voffset;		/* offset in virtual chip */
	int	fd;			/* native gpio line fd */
	int	dir;			/* gpio direction */
	bool	busy;			/* gpio line request by kernel */
	struct native_gpio_chip	*chip;	/* parent gpio chip */
};

struct native_gpio_chip {
	char	name[32];		/* gpio chip name */
	char	label[32];		/* gpio chip label name */
	char	dev_name[32];		/* device node name */
	int	fd;			/* native gpio chip fd */
	uint32_t		ngpio;	/* gpio line numbers */
	struct gpio_line	*lines;	/* gpio lines in the chip */
};

struct virtio_gpio {
	pthread_mutex_t	mtx;
	struct virtio_base	base;
	struct virtio_vq_info	queues[VIRTIO_GPIO_MAXQ];
	struct native_gpio_chip	chips[VIRTIO_GPIO_MAX_CHIPS];
	uint32_t		nchip;
	struct gpio_line	*vlines[VIRTIO_GPIO_MAX_VLINES];
	uint32_t		nvline;
	struct virtio_gpio_config	config;
};

static void
native_gpio_update_line_info(struct gpio_line *line)
{
	struct gpioline_info info;
	int rc;

	memset(&info, 0, sizeof(info));
	info.line_offset = line->offset;
	rc = ioctl(line->chip->fd, GPIO_GET_LINEINFO_IOCTL, &info);
	if (rc) {
		DPRINTF("ioctl GPIO_GET_LINEINFO_IOCTL error %s\n",
				strerror(errno));
		return;
	}

	line->busy = info.flags & GPIOLINE_FLAG_KERNEL;

	/*
	 * if it is already used by virtio gpio model,
	 * it is not set to busy state
	 */
	if (line->fd > 0)
		line->busy = false;

	/* 0 means output, 1 means input */
	line->dir = info.flags & GPIOLINE_FLAG_IS_OUT ? 0 : 1;
	strncpy(line->name, info.name, sizeof(line->name) - 1);
}

static int
native_gpio_open_line(struct gpio_line *line, unsigned int flags,
		unsigned int value)
{
	struct gpiohandle_request req;
	int rc;

	memset(&req, 0, sizeof(req));
	req.lineoffsets[0] = line->offset;
	req.lines = 1;
	strncpy(req.consumer_label, "acrn_dm", sizeof(req.consumer_label) - 1);
	if (flags) {
		req.flags = flags;
		if (flags & GPIOHANDLE_REQUEST_OUTPUT)
			req.default_values[0] = value;
	}
	rc = ioctl(line->chip->fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
	if (rc < 0) {
		DPRINTF("ioctl GPIO_GET_LINEHANDLE_IOCTL error %s\n",
				strerror(errno));
		return -1;
	}

	line->fd = req.fd;
	return 0;
}

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
native_gpio_open_chip(struct native_gpio_chip *chip, const char *name)
{
	struct gpiochip_info info;
	struct gpio_line *line;
	char path[64] = {0};
	int fd, rc, i;

	snprintf(path, sizeof(path), "/dev/%s", name);
	fd = open(path, O_RDWR);
	if (fd < 0) {
		DPRINTF("Can't open gpio device: %s, error %s\n",
				path, strerror(errno));
		return -1;
	}

	memset(&info, 0, sizeof(info));
	rc = ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info);
	if (rc < 0) {
		DPRINTF("Can't ioctl gpio device: %s, error %s\n",
				path, strerror(errno));
		goto fail;
	}

	chip->lines = calloc(1, info.lines * sizeof(*chip->lines));
	if (!chip->lines) {
		DPRINTF("Alloc chip lines error, %s:%d, error %s\n",
				path, chip->ngpio, strerror(errno));
		goto fail;
	}
	chip->fd = fd;
	chip->ngpio = info.lines;
	strncpy(chip->name, info.name, sizeof(chip->name) - 1);
	strncpy(chip->label, info.label, sizeof(chip->label) - 1);
	strncpy(chip->dev_name, name, sizeof(chip->dev_name) - 1);

	/* initialize all lines of the chip */
	for (i = 0; i < chip->ngpio; i++) {
		line = &chip->lines[i];
		line->offset = i;
		line->chip = chip;

		/*
		 * The line's fd and voffset will be initialized
		 * when virtual gpio line connects to the real line.
		 */
		line->fd = -1;
		line->voffset = -1;

		/* Set line state and name via ioctl*/
		native_gpio_update_line_info(line);
	}

	return 0;

fail:
	if (fd > 0)
		close(fd);
	chip->fd = -1;
	chip->ngpio = 0;
	return -1;
}

static void
native_gpio_close_chip(struct native_gpio_chip *chip)
{
	if (chip) {
		memset(chip->name, 0, sizeof(chip->name));
		memset(chip->label, 0, sizeof(chip->label));
		memset(chip->dev_name, 0, sizeof(chip->dev_name));
		if (chip->fd > 0) {
			close(chip->fd);
			chip->fd = -1;
		}
		if (chip->lines) {
			free(chip->lines);
			chip->lines = NULL;
		}
		chip->ngpio = 0;
	}
}

static int
native_gpio_get_offset(struct native_gpio_chip *chip, char *name)
{
	int rc;
	int i;

	/* try to find a gpio index by offset or name */
	if (isalpha(name[0])) {
		for (i = 0; i < chip->ngpio; i++) {
			if (!strcmp(chip->lines[i].name, name))
				return i;
		}
	} else if (isdigit(name[0])) {
		rc = dm_strtoi(name, NULL, 10, &i);
		if (rc == 0 && i < chip->ngpio)
			return i;
	}
	return -1;
}

static struct gpio_line *
native_gpio_find_line(struct native_gpio_chip *chip, const char *name)
{
	int offset;
	char *b, *o, *c;
	struct gpio_line *line = NULL;

	b = o = strdup(name);
	c = strsep(&o, "=");

	/* find the line's offset in the chip by name or number */
	offset = native_gpio_get_offset(chip, c);
	if (offset < 0) {
		DPRINTF("the %s line has not been found in %s chip\n",
				c, chip->dev_name);
		goto out;
	}

	line = &chip->lines[offset];
	if (line->busy || native_gpio_open_line(line, 0, 0) < 0) {
		line = NULL;
		goto out;
	}

	/* If the user sets the name of the GPIO, copy it to vname */
	if (o)
		strncpy(line->vname, o, sizeof(line->vname) - 1);

out:
	free(b);
	return line;
}


static int
native_gpio_init(struct virtio_gpio *gpio, char *opts)
{
	struct gpio_line *line;
	char *cstr, *lstr, *tmp, *b, *o;
	int rc;
	int cn = 0;
	int ln = 0;

	/*
	 * -s <slot>,virtio-gpio,<gpio resources>
	 * <gpio resources> format
	 * <@chip_name0{offset|name[=vname]:offset|name[=vname]:...}
	 * [@chip_name1{offset|name[=vname]:offset|name[=vname]:...}]
	 * [@chip_name2{offset|name[=vname]:offset|name[=vname]:...}]
	 * ...>
	 */

	b = o = strdup(opts);
	while ((tmp = strsep(&o, "@")) != NULL) {

		/* discard subsequent chips */
		if (cn >= VIRTIO_GPIO_MAX_CHIPS ||
				ln >= VIRTIO_GPIO_MAX_VLINES) {
			DPRINTF("gpio chips or lines reach max, cn %d, ln %d\n",
					cn, ln);
			break;
		}

		/* ignore the null string */
		if (tmp[0] == '\0')
			continue;

		/*
		 * parse gpio chip name
		 * if there is no gpiochip information, like "@{...}"
		 * ignore all of the lines.
		 */
		cstr = strsep(&tmp, "{");
		if (!tmp || !cstr || cstr[0] == '\0')
			continue;

		/* get chip information with its name */
		rc = native_gpio_open_chip(&gpio->chips[cn], cstr);
		if (rc < 0)
			continue;

		/* parse all gpio lines in one chip */
		cstr = strsep(&tmp, "}");
		while ((lstr = strsep(&cstr, ":")) != NULL) {

			/* safety check, to avoid "@gpiochip0{::0:1...}" */
			if (lstr[0] == '\0')
				continue;

			/* discard subsequent lines */
			if (ln >= VIRTIO_GPIO_MAX_VLINES) {
				DPRINTF("Virtual gpio lines reach max:%d\n",
						ln);
				break;
			}

			/*
			 * If the line provided by gpio command line is found
			 * assign one virtual gpio offset for it.
			 */
			line = native_gpio_find_line(&gpio->chips[cn], lstr);
			if (line) {
				gpio->vlines[ln] = line;
				line->voffset = ln;
				ln++;
			}
		}
		cn++;
	}
	gpio->nchip = cn;
	gpio->nvline = ln;
	free(b);
	return ln == 0 ? -1 : 0;
}

static int
virtio_gpio_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_gpio *gpio;
	pthread_mutexattr_t attr;
	int rc, i;

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

	rc = native_gpio_init(gpio, opts);
	if (rc) {
		DPRINTF("%s", "virtio gpio: failed to initialize gpio\n");
		goto gpio_fail;
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

	/* gpio numbers for frontend gpio chip */
	gpio->config.ngpio = gpio->nvline;

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
	for (i = 0; i < gpio->nchip; i++)
		native_gpio_close_chip(&gpio->chips[i]);

gpio_fail:
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
	int i;

	DPRINTF("%s", "virtio gpio: pci_gpio_deinit\r\n");
	virtio_gpio_is_active = false;
	gpio = (struct virtio_gpio *)dev->arg;
	if (gpio) {
		pthread_mutex_destroy(&gpio->mtx);
		for (i = 0; i < gpio->nchip; i++)
			native_gpio_close_chip(&gpio->chips[i]);
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
