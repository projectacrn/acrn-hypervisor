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
#include "gpio_dm.h"

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

/*
 *  GPIO IRQ virtualization architecture
 *
 *               SOS                                         UOS
 *  +-------------------------------+
 *  |      virtio GPIO mediator     |
 *  | +-------------------------+   |
 *  | |     GPIO IRQ chip       |   | IRQ chip
 *  | | +-------------------+   |   | virtqueue
 *  | | |Enable, Disable    +<--|---|-----------+
 *  | | |Mask, Unmask, Ack  |   |   |           |
 *  | | +-------------------+   |   | IRQ event |
 *  | | +--------------+        |   | virtqueue |
 *  | | | Generate IRQ +--------|---|--------+  |
 *  | | +--------------+        |   |        |  |
 *  | +-------------------------+   |        |  |
 *  |   ^    ^    ^    ^            |        |  |
 *  +---|----|----|----|------------+        |  |
 * -----|----|----|----|---------------------+--+-------------------------------
 *    +-+----+----+----+-+                   |  | +------------+  +------------+
 *    | gpiolib framework|                   |  | |IRQ consumer|  |IRQ consumer|
 *    +------------------+                   |  | +------------+  +------------+
 *                                           |  | +----------------------------+
 *                                           |  | |   UOS gpiolib framework    |
 *                                           |  | +----------------------------+
 *                                           |  | +----------------------+
 *                                           |  +-+   UOS virtio GPIO    |
 *                                           +--->|   IRQ chip           |
 *                                                +----------------------+
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

#define BIT(x) (1 << (x))

/* Virtio GPIO supports maximum number of virtual gpio */
#define VIRTIO_GPIO_MAX_VLINES	64

/* Virtio GPIO supports maximum native gpio chips */
#define VIRTIO_GPIO_MAX_CHIPS	8

/* Virtio GPIO virtqueue numbers*/
#define VIRTIO_GPIO_MAXQ	3

/* Virtio GPIO capabilities */
#define VIRTIO_GPIO_F_CHIP	1
#define VIRTIO_GPIO_S_HOSTCAPS	VIRTIO_GPIO_F_CHIP

#define IRQ_TYPE_NONE		0
#define IRQ_TYPE_EDGE_RISING	(1 << 0)
#define IRQ_TYPE_EDGE_FALLING	(1 << 1)
#define IRQ_TYPE_LEVEL_HIGH	(1 << 2)
#define IRQ_TYPE_LEVEL_LOW	(1 << 3)
#define IRQ_TYPE_EDGE_BOTH	(IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)
#define IRQ_TYPE_LEVEL_MASK	(IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH)

/* make virtio gpio mediator a singleton mode */
static bool virtio_gpio_is_active;

/* GPIO PIO space */
#define GPIO_PIO_SIZE	(VIRTIO_GPIO_MAX_VLINES * 4)
static uint64_t gpio_pio_start;

/* Uses the same packed config format as generic pinconf. */
#define PIN_CONF_UNPACKED(p) ((unsigned long) p & 0xffUL)
enum pin_config_param {
	PIN_CONFIG_BIAS_BUS_HOLD,
	PIN_CONFIG_BIAS_DISABLE,
	PIN_CONFIG_BIAS_HIGH_IMPEDANCE,
	PIN_CONFIG_BIAS_PULL_DOWN,
	PIN_CONFIG_BIAS_PULL_PIN_DEFAULT,
	PIN_CONFIG_BIAS_PULL_UP,
	PIN_CONFIG_DRIVE_OPEN_DRAIN,
	PIN_CONFIG_DRIVE_OPEN_SOURCE,
	PIN_CONFIG_DRIVE_PUSH_PULL,
	PIN_CONFIG_DRIVE_STRENGTH,
	PIN_CONFIG_INPUT_DEBOUNCE,
	PIN_CONFIG_INPUT_ENABLE,
	PIN_CONFIG_INPUT_SCHMITT,
	PIN_CONFIG_INPUT_SCHMITT_ENABLE,
	PIN_CONFIG_LOW_POWER_MODE,
	PIN_CONFIG_OUTPUT_ENABLE,
	PIN_CONFIG_OUTPUT,
	PIN_CONFIG_POWER_SOURCE,
	PIN_CONFIG_SLEEP_HARDWARE_STATE,
	PIN_CONFIG_SLEW_RATE,
	PIN_CONFIG_END = 0x7F,
	PIN_CONFIG_MAX = 0xFF,
};

enum virtio_gpio_request_command {
	GPIO_REQ_SET_VALUE		= 0,
	GPIO_REQ_GET_VALUE		= 1,
	GPIO_REQ_INPUT_DIRECTION	= 2,
	GPIO_REQ_OUTPUT_DIRECTION	= 3,
	GPIO_REQ_GET_DIRECTION		= 4,
	GPIO_REQ_SET_CONFIG		= 5,

	GPIO_REQ_MAX
};

enum gpio_irq_action {
	IRQ_ACTION_ENABLE   = 0,
	IRQ_ACTION_DISABLE,
	IRQ_ACTION_ACK,
	IRQ_ACTION_MASK,
	IRQ_ACTION_UNMASK,

	IRQ_ACTION_MAX
};

struct virtio_gpio_request {
	uint8_t	cmd;
	uint8_t	offset;
	uint64_t	data;
} __attribute__((packed));

struct virtio_gpio_response {
	int8_t	err;
	uint8_t	data;
} __attribute__((packed));

struct virtio_gpio_info {
	struct virtio_gpio_request	req;
	struct virtio_gpio_response	rsp;
} __attribute__((packed));

struct virtio_gpio_data {
	char	name[32];
} __attribute__((packed));

struct virtio_gpio_config {
	uint16_t	base;	/* base number */
	uint16_t	ngpio;	/* number of gpios */
} __attribute__((packed));

struct virtio_gpio_irq_request {
	uint8_t action;
	uint8_t pin;
	uint8_t mode;
} __attribute__((packed));


struct gpio_line {
	char	name[32];		/* native gpio name */
	char	vname[32];		/* virtual gpio name */
	int	offset;			/* offset in real chip */
	int	voffset;		/* offset in virtual chip */
	int	fd;			/* native gpio line fd */
	int	dir;			/* gpio direction */
	bool	busy;			/* gpio line request by kernel */
	int	value;			/* gpio value */
	uint64_t		config;	/* gpio configuration */
	struct native_gpio_chip	*chip;	/* parent gpio chip */
	struct gpio_irq_desc	*irq;	/* connect to irq descriptor */
};

struct native_gpio_chip {
	char	name[32];		/* gpio chip name */
	char	label[32];		/* gpio chip label name */
	char	dev_name[32];		/* device node name */
	int	fd;			/* native gpio chip fd */
	uint32_t		ngpio;	/* gpio line numbers */
	struct gpio_line	*lines;	/* gpio lines in the chip */
};

struct gpio_irq_desc {
	struct gpio_line	*gpio;	/* connect to gpio line */
	struct mevent		*mevt;	/* mevent for event report */
	int			fd;	/* read event */
	int			pin;	/* pin number */
	bool			mask;	/* mask and unmask */
	bool			deinit;	/* deinit state */
	uint8_t			level;	/* level value */
	uint64_t		mode;		/* interrupt trigger mode */
	void			*data;		/* virtio gpio instance */
	uint64_t		intr_stat;	/* interrupts count */
};

struct gpio_irq_chip {
	pthread_mutex_t		intr_mtx;
	struct gpio_irq_desc	descs[VIRTIO_GPIO_MAX_VLINES];
	uint64_t		intr_pending;	/* pending interrupts */
	uint64_t		intr_service;	/* service interrupts */
	uint64_t		intr_stat;	/* all interrupts count */
};

struct virtio_gpio {
	struct virtio_base	base;
	pthread_mutex_t	mtx;
	struct virtio_vq_info	queues[VIRTIO_GPIO_MAXQ];
	struct native_gpio_chip	chips[VIRTIO_GPIO_MAX_CHIPS];
	uint32_t		nchip;
	struct gpio_line	*vlines[VIRTIO_GPIO_MAX_VLINES];
	uint32_t		nvline;
	struct virtio_gpio_config	config;
	struct gpio_irq_chip		irq_chip;
};

static void print_gpio_info(struct virtio_gpio *gpio);
static void print_virtio_gpio_info(struct virtio_gpio_request *req,
		struct virtio_gpio_response *rsp, bool in);
static void print_intr_statistics(struct gpio_irq_chip *chip);
static void record_intr_statistics(struct gpio_irq_chip *chip, uint64_t mask);
static void gpio_pio_write(struct virtio_gpio *gpio, int n, uint64_t reg);
static uint32_t gpio_pio_read(struct virtio_gpio *gpio, int n);

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

static void
native_gpio_close_line(struct gpio_line *line)
{
	if (line->fd > 0) {
		close(line->fd);
		line->fd = -1;
	}
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

		/*
		 * before setting a new flag, it needs to try to close the fd
		 * that has been opened first, otherwise it will not be ioctl
		 * successfully.
		 */
		native_gpio_close_line(line);

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

	if (flags) {
		line->config = flags;
		if (flags & GPIOHANDLE_REQUEST_OUTPUT) {
			line->dir = 0;
			line->value = value;
		} else if (flags & GPIOHANDLE_REQUEST_INPUT)
			line->dir = 1;
	}

	line->fd = req.fd;
	return rc;
}

static int
gpio_set_value(struct virtio_gpio *gpio, unsigned int offset,
		unsigned int value)
{
	struct gpio_line *line;
	struct gpiohandle_data data;
	int rc;

	line = gpio->vlines[offset];
	if (line->busy || line->fd < 0) {
		DPRINTF("failed to set gpio%d value, busy:%d, fd:%d\n",
				offset, line->busy, line->fd);
		return -1;
	}

	memset(&data, 0, sizeof(data));
	data.values[0] = value;
	rc = ioctl(line->fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
	if (rc < 0) {
		DPRINTF("ioctl GPIOHANDLE_SET_LINE_VALUES_IOCTL error %s\n",
				strerror(errno));
		return -1;
	}
	line->value = value;
	return 0;
}

static int
gpio_get_value(struct virtio_gpio *gpio, unsigned int offset)
{
	struct gpio_line *line;
	struct gpiohandle_data data;
	int rc, fd;

	line = gpio->vlines[offset];
	if (line->busy) {
		DPRINTF("failed to get gpio %d value, it is busy\n", offset);
		return -1;
	}

	fd = line->fd;
	if (fd < 0) {

		/*
		 * if the GPIO line has configured as IRQ mode, then can't use
		 * gpio line fd to get its value, instead, use IRQ fd to get
		 * the value.
		 */
		if (line->irq->fd < 0) {
			DPRINTF("failed to get gpio %d value, fd is invalid\n",
				offset);
			return -1;
		}
		fd = line->irq->fd;
	}

	memset(&data, 0, sizeof(data));
	rc = ioctl(fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
	if (rc < 0) {
		DPRINTF("ioctl GPIOHANDLE_GET_LINE_VALUES_IOCTL error %s\n",
				strerror(errno));
		return -1;
	}
	return data.values[0];
}

static int
gpio_set_direction_input(struct virtio_gpio *gpio, unsigned int offset)
{
	struct gpio_line *line;

	line = gpio->vlines[offset];
	return native_gpio_open_line(line, GPIOHANDLE_REQUEST_INPUT, 0);
}

static int
gpio_set_direction_output(struct virtio_gpio *gpio, unsigned int offset,
		int value)
{
	struct gpio_line *line;

	line = gpio->vlines[offset];
	return native_gpio_open_line(line, GPIOHANDLE_REQUEST_OUTPUT, value);
}

static int
gpio_get_direction(struct virtio_gpio *gpio, unsigned int offset)
{
	struct gpio_line *line;

	line = gpio->vlines[offset];
	native_gpio_update_line_info(line);
	return line->dir;
}

static int
gpio_set_config(struct virtio_gpio *gpio, unsigned int offset,
		unsigned long config)
{
	struct gpio_line *line;

	line = gpio->vlines[offset];

	/*
	 * gpio userspace API can only provide GPIOLINE_FLAG_OPEN_DRAIN
	 * and GPIOLINE_FLAG_OPEN_SOURCE settings currenlty, other
	 * settings will return an success.
	 */
	config = PIN_CONF_UNPACKED(config);
	switch (config) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		config = GPIOLINE_FLAG_OPEN_DRAIN;
		break;
	case PIN_CONFIG_DRIVE_OPEN_SOURCE:
		config = GPIOLINE_FLAG_OPEN_SOURCE;
		break;
	default:
		return 0;
	}

	return native_gpio_open_line(line, config, 0);
}

static void
gpio_request_handler(struct virtio_gpio *gpio, struct virtio_gpio_request *req,
		struct virtio_gpio_response *rsp)
{
	int rc;

	if (req->cmd >= GPIO_REQ_MAX || req->offset >= gpio->nvline) {
		DPRINTF("discards the gpio request, command:%u, offset:%u\n",
				req->cmd, req->offset);
		rsp->err = -1;
		return;
	}

	print_virtio_gpio_info(req, rsp, true);
	switch (req->cmd) {
	case GPIO_REQ_SET_VALUE:
		rc = gpio_set_value(gpio, req->offset, req->data);
		break;
	case GPIO_REQ_GET_VALUE:
		rc = gpio_get_value(gpio, req->offset);
		if (rc >= 0)
			rsp->data = rc;
		break;
	case GPIO_REQ_OUTPUT_DIRECTION:
		rc = gpio_set_direction_output(gpio, req->offset,
				req->data);
		break;
	case GPIO_REQ_INPUT_DIRECTION:
		rc = gpio_set_direction_input(gpio, req->offset);
		break;
	case GPIO_REQ_GET_DIRECTION:
		rc = gpio_get_direction(gpio, req->offset);
		if (rc >= 0)
			rsp->data = rc;
		break;
	case GPIO_REQ_SET_CONFIG:
		rc = gpio_set_config(gpio, req->offset, req->data);
		break;
	default:
		DPRINTF("invalid gpio request command:%d\n", req->cmd);
		rc = -1;
		break;
	}

	rsp->err = rc < 0 ? -1 : 0;
	print_virtio_gpio_info(req, rsp, false);
}

static void virtio_gpio_reset(void *vdev)
{
	struct virtio_gpio *gpio;

	gpio = vdev;

	DPRINTF("%s", "virtio_gpio: device reset requested!\n");
	virtio_reset_dev(&gpio->base);
}

static int
virtio_gpio_cfgwrite(void *vdev, int offset, int size, uint32_t value)
{
	int cfg_size;

	cfg_size = sizeof(struct virtio_gpio_config);
	offset -= cfg_size;
	if (offset < 0 || offset >= GPIO_PIO_SIZE) {
		DPRINTF("virtio_gpio: write to invalid reg %d\n", offset);
		return -1;
	}

	gpio_pio_write((struct virtio_gpio *)vdev, offset >> 2, value);
	return 0;
}

static int
virtio_gpio_cfgread(void *vdev, int offset, int size, uint32_t *retval)
{
	struct virtio_gpio *gpio = vdev;
	void *ptr;
	int cfg_size;
	uint32_t reg;

	cfg_size = sizeof(struct virtio_gpio_config);
	if (offset < 0 || offset >= cfg_size + GPIO_PIO_SIZE) {
		DPRINTF("virtio_gpio: read from invalid reg %d\n", offset);
		return -1;
	} else if (offset < cfg_size) {
		ptr = (uint8_t *)&gpio->config + offset;
		memcpy(retval, ptr, size);
	} else {
		reg = gpio_pio_read(gpio, (offset - cfg_size) >> 2);
		memcpy(retval, &reg, size);
	}
	return 0;
}

static struct virtio_ops virtio_gpio_ops = {
	"virtio_gpio",			/* our name */
	VIRTIO_GPIO_MAXQ,		/* we support VTGPIO_MAXQ virtqueues */
	sizeof(struct virtio_gpio_config), /* config reg size */
	virtio_gpio_reset,		/* reset */
	NULL,				/* device-wide qnotify */
	virtio_gpio_cfgread,		/* read virtio config */
	virtio_gpio_cfgwrite,		/* write virtio config */
	NULL,				/* apply negotiated features */
	NULL,				/* called on guest set status */

};

static int
virtio_gpio_proc(struct virtio_gpio *gpio, struct iovec *iov, int n)
{
	struct virtio_gpio_data *data;
	struct virtio_gpio_request *req;
	struct virtio_gpio_response *rsp;
	struct gpio_line *line;
	int i, len, rc;

	if (n == 1) { /* provide gpio names for front-end driver */
		data = iov[0].iov_base;
		len = iov[0].iov_len;
		assert(len == gpio->nvline * sizeof(*data));

		for (i = 0; i < gpio->nvline; i++) {
			line = gpio->vlines[i];

			/*
			 * if the user provides the name of gpios in the
			 * command line paremeter, then provide it to UOS,
			 * otherwise provide the physical name of gpio to UOS.
			 */
			if (strlen(line->vname))
				strncpy(data[i].name, line->vname,
						sizeof(data[0].name) - 1);
			else if (strlen(line->name))
				strncpy(data[i].name, line->name,
						sizeof(data[0].name) - 1);
		}
		rc = gpio->nvline;
	} else if (n == 2) { /* handle gpio operations requests */
		req = iov[0].iov_base;
		len = iov[0].iov_len;
		assert(len == sizeof(*req));

		rsp = iov[1].iov_base;
		len = iov[1].iov_len;
		assert(len == sizeof(*rsp));

		gpio_request_handler(gpio, req, rsp);
		rc = sizeof(*rsp);
	} else {
		DPRINTF("virtio gpio: number of buffer error %d\n", n);
		rc = 0;
	}

	return rc;
}

static void
virtio_gpio_notify(void *vdev, struct virtio_vq_info *vq)
{
	struct iovec iov[2];
	struct virtio_gpio *gpio;
	uint16_t idx;
	int n, len;

	gpio = (struct virtio_gpio *)vdev;
	if (vq_has_descs(vq)) {
		n = vq_getchain(vq, &idx, iov, 2, NULL);
		assert(n < 3);

		len = virtio_gpio_proc(gpio, iov, n);
		/*
		 * Release this chain and handle more
		 */
		vq_relchain(vq, idx, len);

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

static void
gpio_irq_deliver_intr(struct virtio_gpio *gpio, uint64_t mask)
{
	struct virtio_vq_info *vq;
	struct iovec iov[1];
	uint16_t idx;
	uint64_t *data;

	vq = &gpio->queues[2];
	if (vq_has_descs(vq) && mask) {
		vq_getchain(vq, &idx, iov, 1, NULL);
		data = iov[0].iov_base;
		assert(sizeof(*data) == iov[0].iov_len);

		*data = mask;

		/*
		 * Release this chain and handle more
		 */
		vq_relchain(vq, idx, sizeof(*data));

		/* Generate interrupt if appropriate. */
		vq_endchains(vq, 1);

		/* interrupt statistics */
		record_intr_statistics(&gpio->irq_chip, mask);

	} else
		DPRINTF("virtio gpio failed to send an IRQ, mask %lu", mask);
}

static void
gpio_irq_generate_intr(struct virtio_gpio *gpio, int pin)
{
	struct gpio_irq_chip *chip;
	struct gpio_irq_desc *desc;

	chip = &gpio->irq_chip;
	desc = &chip->descs[pin];

	/* Ignore interrupt until it is unmasked */
	if (desc->mask)
		return;

	pthread_mutex_lock(&chip->intr_mtx);

	/* set it to pending mask */
	chip->intr_pending |= BIT(pin);

	/*
	 * if all interrupts in service are acknowledged, then send pending
	 * interrupts.
	 */
	if (!chip->intr_service) {
		chip->intr_service = chip->intr_pending;
		chip->intr_pending = 0;

		/* deliver interrupt */
		gpio_irq_deliver_intr(gpio, chip->intr_service);
	}
	pthread_mutex_unlock(&chip->intr_mtx);
}

static void
gpio_irq_set_pin_state(int fd __attribute__((unused)),
		enum ev_type t __attribute__((unused)),
		void *arg)
{
	struct gpioevent_data data;
	struct virtio_gpio *gpio;
	struct gpio_irq_desc *desc;
	int last_level, err;

	assert(arg != NULL);
	desc = (struct gpio_irq_desc *) arg;
	gpio = (struct virtio_gpio *) desc->data;
	last_level = desc->level;

	/* get pin state */
	memset(&data, 0, sizeof(data));
	err = read(desc->fd, &data, sizeof(data));
	if (err != sizeof(data)) {
		DPRINTF("virtio gpio, gpio mevent read error %s, len %d\n",
				strerror(errno), err);
		return;
	}

	if (data.id == GPIOEVENT_EVENT_RISING_EDGE) {

		/* pin level is high */
		desc->level = 1;

		/* jitter protection */
		if (((desc->mode & IRQ_TYPE_EDGE_RISING) && (last_level == 0))
				|| (desc->mode & IRQ_TYPE_LEVEL_HIGH)) {
			gpio_irq_generate_intr(gpio, desc->pin);
		}
	} else if (data.id == GPIOEVENT_EVENT_FALLING_EDGE) {

		/* pin level is low */
		desc->level = 0;

		/* jitter protection */
		if (((desc->mode & IRQ_TYPE_EDGE_FALLING) && (last_level == 1))
				|| (desc->mode & IRQ_TYPE_LEVEL_LOW)) {
			gpio_irq_generate_intr(gpio, desc->pin);
		}
	} else
		DPRINTF("virtio gpio, undefined GPIO event id %d\n", data.id);
}

static void
gpio_irq_disable(struct gpio_irq_chip *chip, unsigned int pin)
{
	struct gpio_irq_desc *desc;

	if (pin >= VIRTIO_GPIO_MAX_VLINES) {
		DPRINTF(" gpio irq disable pin %d is invalid\n", pin);
		return;
	}
	desc = &chip->descs[pin];
	DPRINTF("disable IRQ pin %d <-> native chip %s, GPIO %d\n",
		pin, desc->gpio->chip->dev_name, desc->gpio->offset);

	/* Release the mevent, mevent teardown handles IRQ desc reset */
	if (desc->mevt) {
		desc->deinit = false;
		mevent_delete(desc->mevt);
		desc->mevt = NULL;
	}
}

static void
gpio_irq_teardown(void *param)
{
	struct gpio_irq_desc *desc;

	DPRINTF("%s", "virtio gpio tear down\n");
	assert(param != NULL);
	desc = (struct gpio_irq_desc *) param;
	desc->mask = false;
	desc->mode = IRQ_TYPE_NONE;
	if (desc->fd > -1) {
		close(desc->fd);
		desc->fd = -1;
	}

	/* if deinit is not set, switch the pin to GPIO mode */
	if (!desc->deinit)
		native_gpio_open_line(desc->gpio, 0, 0);
}

static void
gpio_irq_enable(struct virtio_gpio *gpio, unsigned int pin,
		uint64_t mode)
{
	struct gpioevent_request req;
	struct gpio_line *line;
	struct gpio_irq_chip *chip;
	struct gpio_irq_desc *desc;
	int err;

	chip = &gpio->irq_chip;
	desc = &chip->descs[pin];
	line = desc->gpio;
	DPRINTF("enable IRQ pin %d, mode %lu <-> chip %s, GPIO %d\n",
		pin, mode, desc->gpio->chip->dev_name, desc->gpio->offset);

	/*
	 * Front-end should set the gpio direction to input before
	 * enable one gpio to irq, so get the gpio value directly
	 * no need to set it to input direction.
	 */
	desc->level = gpio_get_value(gpio, pin);

	/* Release the GPIO line before enable it for IRQ */
	native_gpio_close_line(line);

	memset(&req, 0, sizeof(req));
	if (mode & IRQ_TYPE_EDGE_RISING)
		req.eventflags |= GPIOEVENT_REQUEST_RISING_EDGE;
	if (mode & IRQ_TYPE_EDGE_FALLING)
		req.eventflags |= GPIOEVENT_REQUEST_FALLING_EDGE;

	/*
	 * For level tigger, detect rising and fallling edges to
	 * update the IRQ level value, the value is used to check
	 * the level IRQ is active.
	 */
	if (mode & IRQ_TYPE_LEVEL_MASK)
		req.eventflags |= GPIOEVENT_REQUEST_BOTH_EDGES;
	if (!req.eventflags) {
		DPRINTF("failed to enable pin %d to IRQ with invalid flags\n",
				pin);
		return;
	}

	desc->mode = mode;
	req.lineoffset = line->offset;
	strncpy(req.consumer_label, "acrn_dm_irq",
			sizeof(req.consumer_label) - 1);
	err = ioctl(line->chip->fd, GPIO_GET_LINEEVENT_IOCTL, &req);
	if (err < 0) {
		DPRINTF("ioctl GPIO_GET_LINEEVENT_IOCTL error %s\n",
				strerror(errno));
		goto error;
	}

	desc->fd = req.fd;
	desc->mevt = mevent_add(desc->fd, EVF_READ,
			gpio_irq_set_pin_state, desc,
			gpio_irq_teardown, desc);
	if (!desc->mevt) {
		DPRINTF("failed to enable IRQ pin %d, mevent add error\n",
				pin);
		goto error;
	}

	return;
error:
	gpio_irq_disable(chip, pin);
}

static bool
gpio_irq_has_pending_intr(struct gpio_irq_desc *desc)
{
	bool level_high, level_low;

	/*
	 * For level trigger mode, check the pin mode and level value
	 * to resend an interrupt.
	 */
	if (desc->mode & IRQ_TYPE_LEVEL_MASK) {
		level_high = desc->mode & IRQ_TYPE_LEVEL_HIGH;
		level_low = desc->mode & IRQ_TYPE_LEVEL_LOW;
		if ((level_high && desc->level == 1) ||
				(level_low && desc->level == 0))
			return true;
	}
	return false;
}

static void
gpio_irq_clear_intr(struct gpio_irq_chip *chip, int pin)
{
	pthread_mutex_lock(&chip->intr_mtx);
	chip->intr_service &= ~BIT(pin);
	pthread_mutex_unlock(&chip->intr_mtx);
}

static void
virtio_gpio_irq_proc(struct virtio_gpio *gpio, struct iovec *iov, uint16_t flag)
{
	struct virtio_gpio_irq_request *req;
	struct gpio_irq_chip *chip;
	struct gpio_irq_desc *desc;
	int len;

	req = iov[0].iov_base;
	len = iov[0].iov_len;
	assert(len == sizeof(*req));
	if (req->pin >= gpio->nvline) {
		DPRINTF("virtio gpio, invalid IRQ pin %d, ignore action %d\n",
			req->pin, req->action);
		return;
	}

	chip = &gpio->irq_chip;
	desc = &chip->descs[req->pin];
	switch (req->action) {
	case IRQ_ACTION_ENABLE:
		/*
		 * TODO: need to notify the FE driver
		 *       if gpio_irq_enable failure.
		 */
		gpio_irq_enable(gpio, req->pin, req->mode);

		/* print IRQ statistics */
		print_intr_statistics(chip);
		break;
	case IRQ_ACTION_DISABLE:
		gpio_irq_disable(chip, req->pin);


		/* print IRQ statistics */
		print_intr_statistics(chip);
		break;
	case IRQ_ACTION_ACK:
		/*
		 * For level trigger, we need to check the level value
		 * for next interrupt.
		 */
		gpio_irq_clear_intr(chip, req->pin);
		if (gpio_irq_has_pending_intr(desc))
			gpio_irq_generate_intr(gpio, req->pin);
		break;
	case IRQ_ACTION_MASK:
		desc->mask = true;
		break;
	case IRQ_ACTION_UNMASK:
		desc->mask = false;
		if (gpio_irq_has_pending_intr(desc))
			gpio_irq_generate_intr(gpio, req->pin);
		break;
	default:
		DPRINTF("virtio gpio, unknown IRQ action %d\n", req->action);
	}
}

static void
virtio_irq_evt_notify(void *vdev, struct virtio_vq_info *vq)
{
	/* The front-end driver does not make a kick, just avoid warning */
	DPRINTF("%s", "virtio gpio irq_evt_notify\n");
}

static void
virtio_irq_notify(void *vdev, struct virtio_vq_info *vq)
{
	struct iovec iov[1];
	struct virtio_gpio *gpio;
	uint16_t idx, flag;
	int n;

	gpio = (struct virtio_gpio *)vdev;
	if (vq_has_descs(vq)) {
		n = vq_getchain(vq, &idx, iov, 1, &flag);
		assert(n == 1);

		virtio_gpio_irq_proc(gpio, iov, flag);
		/*
		 * Release this chain and handle more
		 */
		vq_relchain(vq, idx, 1);

		/* Generate interrupt if appropriate. */
		vq_endchains(vq, 1);
	}
}

static void
gpio_irq_deinit(struct virtio_gpio *gpio)
{
	struct gpio_irq_chip *chip;
	struct gpio_irq_desc *desc;
	int i;

	chip = &gpio->irq_chip;
	pthread_mutex_destroy(&chip->intr_mtx);
	for (i = 0; i < gpio->nvline; i++) {
		desc = &chip->descs[i];
		if (desc->mevt) {
			desc->deinit = true;
			mevent_delete(desc->mevt);
			desc->mevt = NULL;

		/* desc fd will be closed by mevent teardown */
		}
	}
}

static int
gpio_irq_init(struct virtio_gpio *gpio)
{
	struct gpio_irq_chip *chip;
	struct gpio_irq_desc *desc;
	struct gpio_line *line;
	int i, rc;

	chip = &gpio->irq_chip;
	rc = pthread_mutex_init(&chip->intr_mtx, NULL);
	if (rc) {
		DPRINTF("IRQ pthread_mutex_init failed with error %d!\n", rc);
		return -1;
	}
	for (i = 0; i < gpio->nvline; i++) {
		desc = &chip->descs[i];
		line = gpio->vlines[i];
		desc->pin = i;
		desc->fd = -1;
		desc->mevt = NULL;
		desc->data = gpio;
		desc->gpio = line;
		line->irq = desc;
	}
	return 0;
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

	rc = gpio_irq_init(gpio);
	if (rc) {
		DPRINTF("%s", "virtio gpio: failed to initialize gpio irq\n");
		goto irq_fail;
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
	gpio->queues[1].qsize = 64;
	gpio->queues[1].notify = virtio_irq_notify;
	gpio->queues[2].qsize = 64;
	gpio->queues[2].notify = virtio_irq_evt_notify;

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

	/* Allocate PIO space for GPIO */
	virtio_gpio_ops.cfgsize += GPIO_PIO_SIZE;

	/* use BAR 0 to map config regs in IO space */
	virtio_set_io_bar(&gpio->base, 0);

	gpio_pio_start = dev->bar[0].addr + VIRTIO_PCI_CONFIG_OFF(1) +
		sizeof(struct virtio_gpio_config);

	virtio_gpio_is_active = true;

	/* dump gpio information */
	print_gpio_info(gpio);
	return 0;

fail:
	pthread_mutex_destroy(&gpio->mtx);

mtx_fail:
	gpio_irq_deinit(gpio);

irq_fail:
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
		gpio_irq_deinit(gpio);
		for (i = 0; i < gpio->nchip; i++)
			native_gpio_close_chip(&gpio->chips[i]);
		free(gpio);
		dev->arg = NULL;
	}

	VIRTIO_GPIO_LOG_DEINIT;
}

static void
virtio_gpio_write_dsdt(struct pci_vdev *dev)
{
	dsdt_line("");
	dsdt_line("Device (AGPI)");
	dsdt_line("{");
	dsdt_line("    Name (_ADR, 0x%04X%04X)", dev->slot, dev->func);
	dsdt_line("    Name (_DDN, \"Virtio GPIO Controller \")");
	dsdt_line("    Name (_UID, One)");
	dsdt_line("    Name (LINK, \"\\\\_SB_.PCI0.AGPI\")");
	dsdt_line("    Method (_CRS, 0, NotSerialized)");
	dsdt_line("    {");
	dsdt_line("    }");
	dsdt_line("}");
	dsdt_line("");
	dsdt_line("Scope (_SB)");
	dsdt_line("{");
	dsdt_line("    Method (%s, 2, Serialized)", PIO_GPIO_CM_SET);
	dsdt_line("    {");
	dsdt_line("        Local0 = (0x%08x + (Arg0 << 2))", gpio_pio_start);
	dsdt_line("        OperationRegion (GPOR, SystemIO, Local0, 0x04)");
	dsdt_line("        Field (GPOR, DWordAcc, NoLock, Preserve)");
	dsdt_line("        {");
	dsdt_line("            TEMP,	2");
	dsdt_line("        }");
	dsdt_line("        TEMP = Arg1");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("    Method (%s, 1, Serialized)", PIO_GPIO_CM_GET);
	dsdt_line("    {");
	dsdt_line("        Local0 = (0x%08x + (Arg0 << 2))", gpio_pio_start);
	dsdt_line("        OperationRegion (GPOR, SystemIO, Local0, 0x04)");
	dsdt_line("        Field (GPOR, DWordAcc, NoLock, Preserve)");
	dsdt_line("        {");
	dsdt_line("            TEMP,    1,");
	dsdt_line("        }");
	dsdt_line("        Return (TEMP)");
	dsdt_line("    }");
	dsdt_line("}");
}

static void
gpio_pio_write(struct virtio_gpio *gpio, int n, uint64_t reg)
{
	struct gpio_line *line;
	int value, dir, mode;
	uint16_t config;

	if (n >= gpio->nvline) {
		DPRINTF("pio write is invalid, n %d, nvline %d\n",
				n, gpio->nvline);
		return;
	}

	line = gpio->vlines[n];
	value = reg & PIO_GPIO_VALUE_MASK;
	dir = (reg & PIO_GPIO_DIR_MASK) >> PIO_GPIO_DIR_OFFSET;
	mode = (reg & PIO_GPIO_MODE_MASK) >> PIO_GPIO_MODE_OFFSET;
	config = (reg & PIO_GPIO_CONFIG_MASK) >> PIO_GPIO_CONFIG_OFFSET;

	/* 0 means GPIO, 1 means IRQ */
	if (mode == 1) {
		DPRINTF("pio write failure, gpio %d is in IRQ mode\n", n);
		return;
	}

	DPRINTF("pio write n %d, reg 0x%lX, val %d, dir %d, mod %d, cfg 0x%x\n",
			n, reg, value, dir, mode, config);

	if (config != line->config)
		gpio_set_config(gpio, n, config);

	/* 0 means output, 1 means input */
	if (dir != line->dir || ((dir == 0) && (value != line->value)))
		dir == 0 ? gpio_set_direction_output(gpio, n, value) :
			gpio_set_direction_input(gpio, n);

}

static uint32_t
gpio_pio_read(struct virtio_gpio *gpio, int n)
{
	struct gpio_line *line;
	uint32_t reg = 0;

	if (n >= gpio->nvline) {
		DPRINTF("pio read is invalid, n %d, nvline %d\n",
				n, gpio->nvline);
		return 0xFFFFFFFF;
	}

	line = gpio->vlines[n];
	reg = line->value;
	reg |= ((line->dir << PIO_GPIO_DIR_OFFSET) & PIO_GPIO_DIR_MASK);
	reg |= ((line->config << PIO_GPIO_CONFIG_OFFSET) &
		PIO_GPIO_CONFIG_MASK);
	if (line->irq->fd > 0)
		reg |= 1 << PIO_GPIO_MODE_OFFSET;

	DPRINTF("pio read n %d, reg 0x%X\n", n, reg);
	return reg;
}

struct pci_vdev_ops pci_ops_virtio_gpio = {
	.class_name	= "virtio-gpio",
	.vdev_init	= virtio_gpio_init,
	.vdev_deinit	= virtio_gpio_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read,
	.vdev_write_dsdt	= virtio_gpio_write_dsdt,
};

static void
print_gpio_info(struct virtio_gpio *gpio)
{
	struct native_gpio_chip *chip;
	struct gpio_line *line;
	int i;

	DPRINTF("=== virtual lines(%u) mapping ===\n", gpio->nvline);
	for (i = 0; i < gpio->nvline; i++) {
		line = gpio->vlines[i];
		DPRINTF("%d: (vname:%8s, name:%8s) <=> %s, gpio=%d\n",
				i,
				line->vname,
				line->name,
				line->chip->dev_name,
				line->offset);
	}

	DPRINTF("=== native gpio chips(%u) info ===\n", gpio->nchip);
	for (i = 0; i < gpio->nchip; i++) {
		chip = &gpio->chips[i];
		DPRINTF("index:%d, name:%s, dev_name:%s, label:%s, ngpio:%u\n",
				i,
				chip->name,
				chip->dev_name,
				chip->label,
				chip->ngpio);
	}
}

static void
print_virtio_gpio_info(struct virtio_gpio_request *req,
		struct virtio_gpio_response *rsp, bool in)
{
	const char *item;
	const char *const cmd_map[GPIO_REQ_MAX + 1] = {
		"GPIO_REQ_SET_VALUE",
		"GPIO_REQ_GET_VALUE",
		"GPIO_REQ_INPUT_DIRECTION",
		"GPIO_REQ_OUTPUT_DIRECTION",
		"GPIO_REQ_GET_DIRECTION",
		"GPIO_REQ_SET_CONFIG",
		"GPIO_REQ_MAX",
	};

	if (req->cmd == GPIO_REQ_SET_VALUE || req->cmd == GPIO_REQ_GET_VALUE)
		item = "value";
	else if (req->cmd == GPIO_REQ_SET_CONFIG)
		item = "config";
	else if (req->cmd == GPIO_REQ_GET_DIRECTION)
		item = "direction";
	else
		item = "data";
	if (in)
		DPRINTF("<<<< gpio=%u, %s, %s=%lu\n",
				req->offset,
				cmd_map[req->cmd],
				item,
				req->data);
	else
		DPRINTF(">>>> gpio=%u, err=%d, %s=%d\n",
				req->offset,
				rsp->err,
				item,
				rsp->data);
}

static void
record_intr_statistics(struct gpio_irq_chip *chip, uint64_t mask)
{
	struct gpio_irq_desc *desc;
	int i;

	for (i = 0; i < VIRTIO_GPIO_MAX_VLINES; i++) {
		desc = &chip->descs[i];
		if (mask & BIT(desc->pin))
			desc->intr_stat++;
	}
	chip->intr_stat++;
}

static void
print_intr_statistics(struct gpio_irq_chip *chip)
{
	struct gpio_irq_desc *desc;
	int i;

	DPRINTF("virtio gpio generated interrupts %lu\n", chip->intr_stat);
	for (i = 0; i < VIRTIO_GPIO_MAX_VLINES; i++) {
		desc = &chip->descs[i];
		if (!desc->gpio || desc->intr_stat == 0)
			continue;
		DPRINTF("Chip %s GPIO %d generated interrupts %lu\n",
				desc->gpio->chip->dev_name, desc->gpio->offset,
				desc->intr_stat);
	}
}

DEFINE_PCI_DEVTYPE(pci_ops_virtio_gpio);
