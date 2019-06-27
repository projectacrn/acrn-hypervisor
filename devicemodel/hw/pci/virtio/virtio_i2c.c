/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sys/param.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <openssl/md5.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"

/* I2c adapter virtualization architecture
 *
 *                +-----------------------------+
 *                | ACRN DM                     |
 *                |  +----------------------+   |  virtqueue
 *                |  |                      |<--+-----------+
 *                |  | virtio i2c mediator  |   |           |
 *                |  |                      |   |           |
 *                |  +--+-----+-----+-------+   |           |
 *                +-----+-----+-----+-----------+           |
 * User space   +-------+     |     +-----------+           |
 *              v             v                 v           |
 *    +---------+----+  +-----+--------+  +-----+------+    |  +-----------+
 * ---+ /dev/i2c-0   +--+ /dev/i2c-1   +--+ /dev/i2c-n +----+--+UOS:       |
 *    |              |                 |  |            |    |  |/dev/i2c-n |
 *    +----------+---+  +-------+------+  +-----+------+    |  +-----+-----+
 * Kernel space  v              v               v           |        v
 *         +-----+-------+ +----+--------+ +----+--------+  |  +-----+------------+
 *         |i2c adapter 0| |i2c adapter 1| |i2c adapter n|  +->|UOS:              |
 *         |             | |             |               |     |virtio i2c adapter|
 *         +-----+-------+ +-------------+ +-------------+     +------------------+
 * --------------+-----------------------------------------
 * Hardware      +----------+
 *               |          |
 *          bus 0v          v      ....
 *         +-----+---+ +----+----+
 *         |i2c slave| |i2c slave| ....
 *         +---------+ +---------+
 */

static int virtio_i2c_debug=0;
#define VIRTIO_I2C_PREF "virtio_i2c: "
#define DPRINTF(fmt, args...) \
	do { if (virtio_i2c_debug) printf(VIRTIO_I2C_PREF fmt, ##args); } while (0)
#define WPRINTF(fmt, args...) printf(VIRTIO_I2C_PREF fmt, ##args)

#define MAX_I2C_VDEV		128
#define MAX_NATIVE_I2C_ADAPTER	16

struct native_i2c_adapter {
	int 		fd;
	int 		bus;
	bool 		i2cdev_enable[MAX_I2C_VDEV];
};

/*
 * Per-device struct
 */
struct virtio_i2c {
	struct virtio_base base;
	pthread_mutex_t mtx;
	struct native_i2c_adapter *native_adapter[MAX_NATIVE_I2C_ADAPTER];
	int native_adapter_num;
	uint16_t adapter_map[MAX_I2C_VDEV];
	struct virtio_vq_info vq;
	char ident[256];
};

static void virtio_i2c_reset(void *);
static void virtio_i2c_notify(void *, struct virtio_vq_info *);

static struct virtio_ops virtio_i2c_ops = {
	"virtio_i2c",		/* our name */
	1,			/* we support 1 virtqueue */
	0, /* config reg size */
	virtio_i2c_reset,	/* reset */
	virtio_i2c_notify,	/* device-wide qnotify */
	NULL,	/* read PCI config */
	NULL,	/* write PCI config */
	NULL,			/* apply negotiated features */
	NULL,			/* called on guest set status */
};

static bool
native_slave_access_ok(struct native_i2c_adapter *adapter, uint16_t addr)
{
	if (ioctl(adapter->fd, I2C_SLAVE, addr) < 0) {
		if (errno == EBUSY) {
			WPRINTF("i2c_core: slave device %x is busy!\n", addr);
		} else {
			WPRINTF("i2c_core: slave device %d is not exsit!\n", addr);
		}
		return false;
	}
	return true;
}

static struct native_i2c_adapter *
native_adapter_create(int bus, uint16_t slave_addr[], int n_slave)
{
	int fd;
	struct native_i2c_adapter *native_adapter;
	char native_path[20];
	int i;

	if (bus < 0)
		return NULL;

	native_adapter = calloc(1, sizeof(struct native_i2c_adapter));
	if (native_adapter == NULL) {
		WPRINTF("i2c_core: failed to calloc struct virtio_i2c_vdev");
		return NULL;
	}

	sprintf(native_path, "/dev/i2c-%d", bus);
	fd = open(native_path, O_RDWR);
	if (fd < 0) {
		WPRINTF("virtio_i2c: failed to open %s\n", native_path);
		return NULL;
	}
	native_adapter->fd = fd;
	native_adapter->bus = bus;
	for (i = 0; i < n_slave; i++) {
		if (slave_addr[i]) {
			if (native_slave_access_ok(native_adapter, slave_addr[i])) {
				if (native_adapter->i2cdev_enable[slave_addr[i]]) {
					WPRINTF("slave addr 0x%x repeat, not allowed.\n", slave_addr[i]);
					goto fail;
				}
				native_adapter->i2cdev_enable[slave_addr[i]] = true;
				DPRINTF("virtio_i2c: add slave 0x%x\n", slave_addr[i]);
			} else {
				goto fail;
			}
		}
	}
	return native_adapter;

fail:
	free(native_adapter);
	return NULL;
}

static void
native_adapter_remove(struct virtio_i2c *vi2c)
{
	int i;
	struct native_i2c_adapter *native_adapter;

	for (i = 0; i < MAX_NATIVE_I2C_ADAPTER; i++) {
		native_adapter = vi2c->native_adapter[i];
		if (native_adapter) {
			if (native_adapter->fd > 0)
				close(native_adapter->fd);
			free(native_adapter);
			vi2c->native_adapter[i] = NULL;
		}
	}
}

static int
virtio_i2c_map(struct virtio_i2c *vi2c)
{
	int i, slave_addr;
	struct native_i2c_adapter *native_adapter;

	/*
	 * Flatten the map for slave address and native adapter to the array:
	 *
	 * adapter_map[MAX_I2C_VDEV]:
	 *
	 * Native Adapter | adapter2 | none  | adapter1 | adapter3 | none | none| (val)
	 *                |----------|-------|----------|----------|------|-----|
	 * Slave Address  | addr 1   | none  | addr 2   | addr 3   | none | none| (idx)
	 *                |<-----------------------MAX_I2C_VDEV---------------->|
	 */
	for (i = 0; i < vi2c->native_adapter_num; i++) {
		native_adapter = vi2c->native_adapter[i];
		for (slave_addr = 0; slave_addr < MAX_I2C_VDEV; slave_addr++) {
			if (native_adapter->i2cdev_enable[slave_addr]) {
				if (vi2c->adapter_map[slave_addr]) {
					WPRINTF("slave addr %x repeat, not support!\n", slave_addr);
					return -1;
				}
				/* As 0 is the initiate value, + 1 for index */
				vi2c->adapter_map[slave_addr] = i + 1;
				DPRINTF("slave:%d -> native adapter: %d \n",
							slave_addr,
							native_adapter->bus);
			}
		}
	}
	return 0;
}

static int
virtio_i2c_parse(struct virtio_i2c *vi2c, char *optstr)
{
	char *cp, *t;
	uint16_t slave_addr[MAX_I2C_VDEV];
	int addr, bus, n_adapter, n_slave;

	/*
	 * virtio-i2c,<bus>:<slave_addr>[:<slave_addr>],
	 * 	[<bus>:<slave_addr>[:<slave_addr>]]
	 *
	 * bus (dec): native adatper bus number.
	 * 	e.g. 2 for /dev/i2c-2
	 * slave_addr (hex): address for native slave device
	 * 	e.g. 0x1C or 1C
	 *
	 * Note: slave address can not repeat.
	 */
	n_adapter = 0;
	while (optstr != NULL) {
		cp = strsep(&optstr, ",");
		/*
		 * <bus>:<slave_addr>[:<slave_addr>]...
		 */
		n_slave = 0;
		bus = -1;
		while (cp != NULL && *cp !='\0') {
			if (*cp == ':')
				cp++;
			if (bus == -1) {
				if (dm_strtoi(cp, &t, 10, &bus) || bus < 0)
					return -1;
			} else {
				if (dm_strtoi(cp, &t, 16, &addr) || addr < 0)
					return -1;
				if (n_slave > MAX_I2C_VDEV) {
					WPRINTF("too many devices, only support %d \n", MAX_I2C_VDEV);
					return -1;
				}
				slave_addr[n_slave] = (uint16_t)(addr & (MAX_I2C_VDEV - 1));
				DPRINTF("native i2c adapter %d:0x%x\n", bus, slave_addr[n_slave]);
				n_slave++;
			}
			cp = t;
		}
		if (n_adapter > MAX_NATIVE_I2C_ADAPTER) {
			WPRINTF("too many adapter, only support %d \n", MAX_NATIVE_I2C_ADAPTER);
			return -1;
		}
		vi2c->native_adapter[n_adapter] = native_adapter_create(bus, slave_addr, n_slave);
		if (!vi2c->native_adapter[n_adapter])
			return -1;
		n_adapter++;
	}
	vi2c->native_adapter_num = n_adapter;

	return 0;
}

static void
virtio_i2c_reset(void *vdev)
{
	struct virtio_i2c *vi2c = vdev;

	DPRINTF("device reset requested !\n");
	virtio_reset_dev(&vi2c->base);
}

static void
virtio_i2c_notify(void *vdev, struct virtio_vq_info *vq)
{
	/* TODO: Add notify logic */
}

static int
virtio_i2c_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	MD5_CTX mdctx;
	u_char digest[16];
	struct virtio_i2c *vi2c;
	pthread_mutexattr_t attr;
	int rc = -1;

	vi2c = calloc(1, sizeof(struct virtio_i2c));
	if (!vi2c) {
		WPRINTF("calloc returns NULL\n");
		return -ENOMEM;
	}

	if (virtio_i2c_parse(vi2c, opts)) {
		WPRINTF("failed to parse parameters %s \n", opts);
		goto mtx_fail;
	}

	if (virtio_i2c_map(vi2c))
		goto mtx_fail;

	/* init mutex attribute properly to avoid deadlock */
	rc = pthread_mutexattr_init(&attr);
	if (rc) {
		WPRINTF("mutexattr init failed with erro %d!\n", rc);
		goto mtx_fail;
	}
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc) {
		WPRINTF("mutexattr_settype failed with "
					"error %d!\n", rc);
		goto mtx_fail;
	}

	rc = pthread_mutex_init(&vi2c->mtx, &attr);
	if (rc) {
		WPRINTF("pthread_mutex_init failed with "
					"error %d!\n", rc);
		goto mtx_fail;
	}

	/* init virtio struct and virtqueues */
	virtio_linkup(&vi2c->base, &virtio_i2c_ops, vi2c, dev, &vi2c->vq, BACKEND_VBSU);
	vi2c->base.mtx = &vi2c->mtx;
	vi2c->vq.qsize = 64;
	vi2c->native_adapter_num = 0;

	MD5_Init(&mdctx);
	MD5_Update(&mdctx, "vi2c", strlen("vi2c"));
	MD5_Final(digest, &mdctx);
	rc = snprintf(vi2c->ident, sizeof(vi2c->ident),
		"ACRN--%02X%02X-%02X%02X-%02X%02X", digest[0],
		digest[1], digest[2], digest[3], digest[4],
		digest[5]);
	if (rc < 0) {
		WPRINTF("create ident failed");
		goto fail;
	}
	if (rc >= sizeof(vi2c->ident)) {
		WPRINTF("ident too long\n");
	}

	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_I2C);
	pci_set_cfgdata16(dev, PCIR_VENDOR, INTEL_VENDOR_ID);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_SERIALBUS);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_I2C);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, INTEL_VENDOR_ID);

	if (virtio_interrupt_init(&vi2c->base, virtio_uses_msix())) {
		WPRINTF("failed to init interrupt");
		rc = -1;
		goto fail;
	}
	virtio_set_io_bar(&vi2c->base, 0);
	return 0;

fail:
	pthread_mutex_destroy(&vi2c->mtx);
mtx_fail:
	native_adapter_remove(vi2c);
	free(vi2c);
	return rc;
}

static void
virtio_i2c_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_i2c *vi2c;

	if (dev->arg) {
		DPRINTF("deinit\n");
		vi2c = (struct virtio_i2c *) dev->arg;
		native_adapter_remove(vi2c);
		pthread_mutex_destroy(&vi2c->mtx);
		free(vi2c);
		dev->arg = NULL;
	}
}

struct pci_vdev_ops pci_ops_virtio_i2c = {
	.class_name		= "virtio-i2c",
	.vdev_init		= virtio_i2c_init,
	.vdev_deinit		= virtio_i2c_deinit,
	.vdev_barwrite		= virtio_pci_write,
	.vdev_barread		= virtio_pci_read,
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_i2c);
