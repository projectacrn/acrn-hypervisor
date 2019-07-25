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
#include "acpi.h"

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

#define MAX_NODE_NAME_LEN	20
#define MAX_I2C_VDEV		128
#define MAX_NATIVE_I2C_ADAPTER	16
#define I2C_MSG_OK	0
#define I2C_MSG_ERR	1
#define I2C_NO_DEV	2

static int acpi_i2c_adapter_num = 0;
static void acpi_add_i2c_adapter(struct pci_vdev *dev, int i2c_bus);
static void acpi_add_cam1(struct pci_vdev *dev, int i2c_bus);
static void acpi_add_cam2(struct pci_vdev *dev, int i2c_bus);
static void acpi_add_hdac(struct pci_vdev *dev, int i2c_bus);
static void acpi_add_default(struct pci_vdev *dev, int i2c_bus);

struct acpi_node {
	char node_name[MAX_NODE_NAME_LEN];
	void (*add_node_fn)(struct pci_vdev *, int);
};

static struct acpi_node acpi_node_table[] = {
/* cam1, cam2 and hdac is dump from MRB board */
	{"cam1", acpi_add_cam1},
	{"cam2", acpi_add_cam2},
	{"hdac", acpi_add_hdac},
	{"default", acpi_add_default},
};

struct virtio_i2c_hdr {
	uint16_t addr;      /* slave address */
	uint16_t flags;
	uint16_t len;       /*msg length*/
}__attribute__((packed));

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
	char acpi_nodes[MAX_I2C_VDEV][MAX_NODE_NAME_LEN];
	struct virtio_vq_info vq;
	char ident[256];
	pthread_t req_tid;
	pthread_mutex_t req_mtx;
	pthread_cond_t req_cond;
	int in_process;
	int closing;
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

static void
acpi_add_i2c_adapter(struct pci_vdev *dev, int i2c_bus)
{
	dsdt_line("Device (I2C%d)", i2c_bus);
	dsdt_line("{");
	dsdt_line("    Name (_ADR, 0x%04X%04X)", dev->slot, dev->func);
	dsdt_line("    Name (_DDN, \"Intel(R) I2C Controller #%d\")", i2c_bus);
	dsdt_line("    Name (_UID, One)");
	dsdt_line("    Name (LINK, \"\\\\_SB.PCI%d.I2C%d\")", dev->bus, i2c_bus);
	dsdt_line("    Name (RBUF, ResourceTemplate ()");
	dsdt_line("    {");
	dsdt_line("    })");
	dsdt_line("    Name (IC0S, 0x00061A80)");
	dsdt_line("    Name (_DSD, Package (0x02)");
	dsdt_line("    {");
	dsdt_line("        ToUUID (\"daffd814-6eba-4d8c-8a91-bc9bbf4aa301\")"
				" ,");
	dsdt_line("        Package (0x01)");
	dsdt_line("        {");
	dsdt_line("            Package (0x02)");
	dsdt_line("            {");
	dsdt_line("                \"clock-frequency\", ");
	dsdt_line("                IC0S");
	dsdt_line("            }");
	dsdt_line("        }");
	dsdt_line("    })");
	dsdt_line("");
	dsdt_line("}");
}

static void
acpi_add_cam1(struct pci_vdev *dev, int i2c_bus)
{
	dsdt_line("Scope(I2C%d)", i2c_bus);
	dsdt_line("{");
	dsdt_line("    Device (CAM1)");
	dsdt_line("    {");
	dsdt_line("        Name (_ADR, Zero)  // _ADR: Address");
	dsdt_line("        Name (_HID, \"ADV7481A\")  // _HID: Hardware ID");
	dsdt_line("        Name (_CID, \"ADV7481A\")  // _CID: Compatible ID");
	dsdt_line("        Name (_UID, One)  // _UID: Unique ID");
	dsdt_line("        Method (_CRS, 0, Serialized)");
	dsdt_line("        {");
	dsdt_line("            Name (SBUF, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("                GpioIo (Exclusive, PullDefault, 0x0000, "
					"0x0000, IoRestrictionInputOnly,");
	dsdt_line("                    \"\\\\_SB.GPO0\", 0x00, "
					"ResourceConsumer, ,");
	dsdt_line("                    )");
	dsdt_line("                    {   // Pin list");
	dsdt_line("                        0x001E");
	dsdt_line("                    }");
	dsdt_line("                I2cSerialBusV2 (0x0070, "
					"ControllerInitiated, 0x00061A80,");
	dsdt_line("                    AddressingMode7Bit, "
						"\"\\\\_SB.PCI%d.I2C%d\",",
							dev->bus, i2c_bus);
	dsdt_line("                    0x00, ResourceConsumer, , Exclusive,");
	dsdt_line("                    )");
	dsdt_line("            })");
	dsdt_line("            Return (SBUF)");
	dsdt_line("        }");
	dsdt_line("        Method (_DSM, 4, NotSerialized)");
	dsdt_line("        {");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"377ba76a-f390-4aff-ab38-9b1bf33a3015\")))");
	dsdt_line("            {");
	dsdt_line("                Return (\"ADV7481A\")");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"ea3b7bd8-e09b-4239-ad6e-ed525f3f26ab\")))");
	dsdt_line("            {");
	dsdt_line("                Return (0x40)");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"8dbe2651-70c1-4c6f-ac87-a37cb46e4af6\")))");
	dsdt_line("            {");
	dsdt_line("                Return (0xFF)");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"26257549-9271-4ca4-bb43-c4899d5a4881\")))");
	dsdt_line("            {");
	dsdt_line("                If (Arg2 == One)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02)");
	dsdt_line("                }");
	dsdt_line("                If (Arg2 == 0x02)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02001000)");
	dsdt_line("                }");
	dsdt_line("                If (Arg2 == 0x03)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02000E01)");
	dsdt_line("                }");
	dsdt_line("            }");
	dsdt_line("            Return (Zero)");
	dsdt_line("        }");
	dsdt_line("    }");
	dsdt_line("}");
}

static void
acpi_add_cam2(struct pci_vdev *dev, int i2c_bus)
{
		dsdt_line("Scope(I2C%d)", i2c_bus);
		dsdt_line("{");
		dsdt_line("    Device (CAM2)");
		dsdt_line("    {");
		dsdt_line("        Name (_ADR, Zero)  // _ADR: Address");
		dsdt_line("        Name (_HID, \"ADV7481B\")  // _HID: Hardware ID");
		dsdt_line("        Name (_CID, \"ADV7481B\")  // _CID: Compatible ID");
		dsdt_line("        Name (_UID, One)  // _UID: Unique ID");
		dsdt_line("        Method (_CRS, 0, Serialized)");
		dsdt_line("        {");
		dsdt_line("            Name (SBUF, ResourceTemplate ()");
		dsdt_line("            {");
		dsdt_line("                GpioIo (Exclusive, PullDefault, 0x000, "
						"0x0000, IoRestrictionInputOnly,");
		dsdt_line("                    \"\\\\_SB.GPO0\", 0x00, "
						"ResourceConsumer, ,");
		dsdt_line("                    )");
		dsdt_line("                    {   // Pin list");
		dsdt_line("                        0x001E");
		dsdt_line("                    }");
		dsdt_line("                I2cSerialBusV2 (0x0071, "
						"ControllerInitiated, 0x00061A80,");
		dsdt_line("                    AddressingMode7Bit, "
							"\"\\\\_SB.PCI%d.I2C%d\",",
								dev->bus, i2c_bus);
		dsdt_line("                    0x00, ResourceConsumer, , Exclusive,");
		dsdt_line("                    )");
		dsdt_line("            })");
		dsdt_line("            Return (SBUF)");
		dsdt_line("        }");
		dsdt_line("        Method (_DSM, 4, NotSerialized) ");
		dsdt_line("        {");
		dsdt_line("            If ((Arg0 == ToUUID ("
					"\"377ba76a-f390-4aff-ab38-9b1bf33a3015\")))");
		dsdt_line("            {");
		dsdt_line("                Return (\"ADV7481B\")");
		dsdt_line("            }");
		dsdt_line("");
		dsdt_line("            If ((Arg0 == ToUUID ("
					"\"ea3b7bd8-e09b-4239-ad6e-ed525f3f26ab\")))");
		dsdt_line("            {");
		dsdt_line("                Return (0x14)");
		dsdt_line("            }");
		dsdt_line("");
		dsdt_line("            If ((Arg0 == ToUUID ("
					"\"8dbe2651-70c1-4c6f-ac87-a37cb46e4af6\")))");
		dsdt_line("            {");
		dsdt_line("                Return (0xFF)");
		dsdt_line("            }");
		dsdt_line("");
		dsdt_line("            If ((Arg0 == ToUUID ("
					"\"26257549-9271-4ca4-bb43-c4899d5a4881\")))");
		dsdt_line("            {");
		dsdt_line("                If (Arg2 == One)");
		dsdt_line("                {");
		dsdt_line("                    Return (0x02)");
		dsdt_line("                }");
		dsdt_line("                If (Arg2 == 0x02)");
		dsdt_line("                {");
		dsdt_line("                    Return (0x02001000)");
		dsdt_line("                }");
		dsdt_line("                If (Arg2 == 0x03)");
		dsdt_line("                {");
		dsdt_line("                    Return (0x02000E01)");
		dsdt_line("                }");
		dsdt_line("            }");
		dsdt_line("            Return (Zero)");
		dsdt_line("        }");
		dsdt_line("    }");
		dsdt_line("");
		dsdt_line("}");
}

static void
acpi_add_hdac(struct pci_vdev *dev, int i2c_bus)
{
	dsdt_line("Scope(I2C%d)", i2c_bus);
	dsdt_line("{");
	dsdt_line("    Device (HDAC)");
	dsdt_line("    {");
	dsdt_line("        Name (_HID, \"INT34C3\")  // _HID: Hardware ID");
	dsdt_line("        Name (_CID, \"INT34C3\")  // _CID: Compatible ID");
	dsdt_line("        Name (_DDN, \"Intel(R) Smart Sound Technology "
			"Audio Codec\")  // _DDN: DOS Device Name");
	dsdt_line("        Name (_UID, One)  // _UID: Unique ID");
	dsdt_line("        Method (_INI, 0, NotSerialized)");
	dsdt_line("        {");
	dsdt_line("        }");
	dsdt_line("");
	dsdt_line("        Method (_CRS, 0, NotSerialized)");
	dsdt_line("        {");
	dsdt_line("            Name (SBFB, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("                I2cSerialBusV2 (0x006C, "
					"ControllerInitiated, 0x00061A80,");
	dsdt_line("                    AddressingMode7Bit, "
						"\"\\\\_SB.PCI%d.I2C%d\",",
							dev->bus, i2c_bus);
	dsdt_line("                    0x00, ResourceConsumer, , Exclusive,");
	dsdt_line("                    )");
	dsdt_line("            })");
	dsdt_line("            Name (SBFI, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("            })");
	dsdt_line("            Return (ConcatenateResTemplate (SBFB, SBFI))");
	dsdt_line("        }");
	dsdt_line("");
	dsdt_line("        Method (_STA, 0, NotSerialized)  // _STA: Status");
	dsdt_line("        {");
	dsdt_line("            Return (0x0F)");
	dsdt_line("        }");
	dsdt_line("    }");
	dsdt_line("}");
}

static void
acpi_add_default(struct pci_vdev *dev, int i2c_bus)
{
	/* Add nothing */
}

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
native_adapter_find(struct virtio_i2c *vi2c, uint16_t addr)
{
	int idx;

	if (addr < MAX_I2C_VDEV && ((idx = vi2c->adapter_map[addr]) != 0)) {
		return vi2c->native_adapter[idx - 1];
	}
	return NULL;
}

static uint8_t
native_adapter_proc(struct virtio_i2c *vi2c, struct i2c_msg *msg)
{
	int ret;
	uint16_t addr;
	struct i2c_rdwr_ioctl_data work_queue;
	struct native_i2c_adapter *adapter;
	uint8_t status;

	addr = msg->addr;
	adapter = native_adapter_find(vi2c, addr);
	if (!adapter)
		return I2C_NO_DEV;

	work_queue.nmsgs = 1;
	work_queue.msgs = msg;

	ret = ioctl(adapter->fd, I2C_RDWR, &work_queue);
	if (ret < 0)
		status = I2C_MSG_ERR;
	else
		status = I2C_MSG_OK;
	if (msg->len)
		DPRINTF("i2c_core: i2c msg: flags=0x%x, addr=0x%x, len=0x%x buf=%x\n",
				msg->flags,
				msg->addr,
				msg->len,
				msg->buf[0]);
	else
		DPRINTF("i2c_core: i2c msg: flags=0x%x, addr=0x%x, len=0x%x\n",
				msg->flags,
				msg->addr,
				msg->len);
	return status;
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

	snprintf(native_path, sizeof(native_path), "/dev/i2c-%d", bus);
	native_path[sizeof(native_path) - 1] = '\0';

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

static void
virtio_i2c_req_stop(struct virtio_i2c *vi2c)
{
	void *jval;

	pthread_mutex_lock(&vi2c->req_mtx);
	vi2c->closing = 1;
	pthread_cond_broadcast(&vi2c->req_cond);
	pthread_mutex_unlock(&vi2c->req_mtx);
	pthread_join(vi2c->req_tid, &jval);
}

static void *
virtio_i2c_proc_thread(void *arg)
{
	struct virtio_i2c *vi2c = arg;
	struct virtio_vq_info *vq = &vi2c->vq;
	struct iovec iov[3];
	uint16_t idx, flags[3];
	struct virtio_i2c_hdr *hdr;
	struct i2c_msg msg;
	uint8_t *status;
	int n;

	for (;;) {
		pthread_mutex_lock(&vi2c->req_mtx);

		vi2c->in_process = 0;
		while (!vq_has_descs(vq) && !vi2c->closing)
			pthread_cond_wait(&vi2c->req_cond, &vi2c->req_mtx);

		if (vi2c->closing) {
			pthread_mutex_unlock(&vi2c->req_mtx);
			return NULL;
		}
		vi2c->in_process = 1;
		pthread_mutex_unlock(&vi2c->req_mtx);
		do {
			n = vq_getchain(vq, &idx, iov, 3, flags);
			if (n < 2 || n > 3) {
				WPRINTF("virtio_i2c_proc: failed to get iov from virtqueue\n");
				continue;
			}
			hdr = iov[0].iov_base;
			msg.addr = hdr->addr;
			msg.flags = hdr->flags;
			if (hdr->len) {
				msg.buf = iov[1].iov_base;
				msg.len = iov[1].iov_len;
				status = iov[2].iov_base;
			} else {
				msg.buf = NULL;
				msg.len = 0;
				status = iov[1].iov_base;
			}
			*status = native_adapter_proc(vi2c, &msg);
			vq_relchain(vq, idx, 1);
		} while (vq_has_descs(vq));
		vq_endchains(vq, 0);
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
	char *cp, *t, *dsdt_str, *p;
	uint16_t slave_addr[MAX_I2C_VDEV];
	int addr, bus, n_adapter, n_slave;

	/*
	 * virtio-i2c,<bus>:<slave_addr[@<node>]>[:<slave_addr[@<node>]>],
	 * 	[<bus>:<slave_addr[@<node>]>[:<slave_addr>[@<node>]]]
	 *
	 * bus (dec): native adatper bus number.
	 * 	e.g. 2 for /dev/i2c-2
	 * slave_addr (hex): address for native slave device
	 * 	e.g. 0x1C or 1C
	 * @<node>: node is the acpi node name defined in acpi_node_table[]
	 * 	e.g. @cam1 means adding 'cam1' node to dsdt table.
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
				p = &vi2c->acpi_nodes[slave_addr[n_slave]][0];
				if (t != NULL && *t == '@') {
					t++;
					dsdt_str = strsep(&t, ":,");
					snprintf(p, MAX_NODE_NAME_LEN, "%s", dsdt_str);
				} else {
					snprintf(p, MAX_NODE_NAME_LEN, "default");
				}
				DPRINTF("native i2c adapter %d:0x%x (%s)\n",
						bus,
						slave_addr[n_slave],
						p);
				n_slave++;
			}
			cp = t;
		}
		if (n_adapter >= MAX_NATIVE_I2C_ADAPTER) {
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
virtio_i2c_dsdt(struct pci_vdev *dev)
{
	int i, j, node_num;
	struct acpi_node *anode;
	int i2c_bus;
	int found;
	struct virtio_i2c *vi2c = (struct virtio_i2c *) dev->arg;

	/* i2c bus number in acpi start from 0 */
	i2c_bus = acpi_i2c_adapter_num;
	/* add i2c adapter */
	acpi_add_i2c_adapter(dev, i2c_bus);
	DPRINTF("add dsdt for i2c adapter %d\n", i2c_bus);

	/* add slave devices */
	node_num = sizeof(acpi_node_table) / sizeof(struct acpi_node);
	for (i = 0; i < MAX_I2C_VDEV; i++) {
		if (!native_adapter_find(vi2c, i))
			continue;

		found = 0;
		for (j = 0; j < node_num; j++) {
			anode = &acpi_node_table[j];
			if (!strncmp(anode->node_name, vi2c->acpi_nodes[i], sizeof(anode->node_name))) {
				found = 1;
				if (anode->add_node_fn) {
					anode->add_node_fn(dev, i2c_bus);
					DPRINTF("add dsdt for %s \n", anode->node_name);
				}
			}
		}
		if (!found)
			WPRINTF("cannot find acpi node info for %s \n", vi2c->acpi_nodes[i]);
	}
	acpi_i2c_adapter_num++;
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
	struct virtio_i2c *vi2c = vdev;

	if (!vq_has_descs(vq))
		return;

	pthread_mutex_lock(&vi2c->req_mtx);
	if (!vi2c->in_process)
		pthread_cond_signal(&vi2c->req_cond);
	pthread_mutex_unlock(&vi2c->req_mtx);
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
	vi2c->in_process = 0;
	vi2c->closing = 0;
	pthread_mutex_init(&vi2c->req_mtx, NULL);
	pthread_cond_init(&vi2c->req_cond, NULL);
	pthread_create(&vi2c->req_tid, NULL, virtio_i2c_proc_thread, vi2c);
	pthread_setname_np(vi2c->req_tid, "virtio-i2c");
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
		virtio_i2c_req_stop(vi2c);
		native_adapter_remove(vi2c);
		pthread_mutex_destroy(&vi2c->req_mtx);
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
	.vdev_write_dsdt	= virtio_i2c_dsdt,
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_i2c);
