/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/*
 * HDCP Virtualization
 *
 *
 *              +---------------+
 *              |               |
 *   +----------+----------+    |    +------------------+
 *   |       ACRN DM       |    |    |     Media APP    |
 *   | +-----------------+ |    |    +------------------+
 *   | |  HDCP Backend   | |    |              |
 *   | +-----------------+ |    |    +------------------+
 *   +---------------------+    |    |  HDCP Libraries  |
 *              |               |    +------------------+
 *              |               |              |
 *     +------------------+     |    +------------------+
 *     | HDCP SOS Daemon  |     |    | HDCP UOS Daemon  |
 *     +------------------+     |    +------------------+
 *                              |
 *    Service OS User Space     |     User OS User Space
 *                              |
 *  --------------------------  |  ---------------------------
 *                              |
 *   Service OS Kernel Space    |    User OS Kernel Space
 *                              |
 *     +------------------+     |    +------------------+
 *     | i915 HDCP Driver |     |    |  HDCP Front End  |
 *     +------------------+     |    +------------------+
 *                              |             |
 *                              +-------------+
 *
 * Above diagram illustrates the HDCP architecture in ACRN. In SOS, HDCP
 * library being used by media app. In UOS, HDCP Daemon gets the HDCP
 * request by open/read/write /dev/hdcp0 which is created by HDCP
 * frontend, instead of accessing GPU. Then the HDCP frontend sends the
 * requests to the HDCP backend thru virtio mechanism. HDCP backend talks to
 * HDCP SOS daemon that will ask HDCP Kernel Driver to execute the requsted
 * operation.
 *
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sysexits.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"

#define NUM_PHYSICAL_PORTS_MAX  5
#define VIRTIO_HDCP_RINGSZ	64
#define VIRTIO_HDCP_NUMQ	1
#define HDCP_DIR_BASE               "/var/run/hdcp/"
#define HDCP_SDK_SOCKET_PATH        HDCP_DIR_BASE ".sdk_socket"

typedef enum _HDCP_API_TYPE
{
	HDCP_API_INVALID,
	HDCP_API_CREATE,
	HDCP_API_DESTROY,
	HDCP_API_ENUMERATE_HDCP_DISPLAY,
	HDCP_API_SENDSRMDATA,
	HDCP_API_GETSRMVERSION,
	HDCP_API_ENABLE,
	HDCP_API_DISABLE,
	HDCP_API_GETSTATUS,
	HDCP_API_GETKSVLIST,
	HDCP_API_REPORTSTATUS,
	HDCP_API_TERM_MSG_LOOP,
	HDCP_API_CREATE_CALLBACK,
	HDCP_API_SET_PROTECTION_LEVEL,
	HDCP_API_CONFIG,
	HDCP_API_ILLEGAL
} HDCP_API_TYPE;

typedef enum _PORT_EVENT
{
	PORT_EVENT_NONE = 0,
	PORT_EVENT_PLUG_IN,         // hot plug in
	PORT_EVENT_PLUG_OUT,        // hot plug out
	PORT_EVENT_LINK_LOST,       // HDCP authentication step3 fail
} PORT_EVENT;

typedef struct _Port
{
	uint32_t Id;
	int Status;
	PORT_EVENT Event;
} Port;

typedef enum _HDCP_STATUS
{
	HDCP_STATUS_SUCCESSFUL = 0,
	HDCP_STATUS_ERROR_ALREADY_CREATED,
	HDCP_STATUS_ERROR_INVALID_PARAMETER,
	HDCP_STATUS_ERROR_NO_DISPLAY,
	HDCP_STATUS_ERROR_REVOKED_DEVICE,
	HDCP_STATUS_ERROR_SRM_INVALID,
	HDCP_STATUS_ERROR_INSUFFICIENT_MEMORY,
	HDCP_STATUS_ERROR_INTERNAL,
	HDCP_STATUS_ERROR_SRM_NOT_RECENT,
	HDCP_STATUS_ERROR_SRM_FILE_STORAGE,
	HDCP_STATUS_ERROR_MAX_DEVICES_EXCEEDED,
	HDCP_STATUS_ERROR_MAX_DEPTH_EXCEEDED,
	HDCP_STATUS_ERROR_MSG_TRANSACTION,
} HDCP_STATUS;

enum HDCP_CONFIG_TYPE
{
	INVALID_CONFIG = 0,             // invalid configure type
	SRM_STORAGE_CONFIG,             // config to disable/enable SRM storage
};

typedef struct _HDCP_CONFIG
{
	enum HDCP_CONFIG_TYPE type;
	bool disableSrmStorage;
} HDCP_CONFIG;

/* HDCP socket data */
struct SocketData
{
	union
	{
		uint8_t Bytes;
		struct
		{
			uint32_t        Size;
			HDCP_API_TYPE   Command;
			HDCP_STATUS     Status;

			uint8_t         KsvCount;   // Number of KSV in topology
			uint8_t         Depth;      // Depth of topology
			bool            isType1Capable; // Port whether support HDCP2.2
			union
			{
				Port        Ports[NUM_PHYSICAL_PORTS_MAX];
				Port        SinglePort;
			};

			uint32_t        PortCount;

			uint32_t        SrmOrKsvListDataSz;
			uint16_t        SrmVersion;

			HDCP_CONFIG     Config;

			uint8_t         Level;
		};
	};
};

/* Per-device struct - VBS-U */
struct virtio_hdcp {
	struct virtio_base		base;
	struct virtio_vq_info		queues[VIRTIO_HDCP_NUMQ];
	pthread_mutex_t			mtx;

	int in_progress;
	pthread_t rx_tid;
	pthread_mutex_t	rx_mtx;
	pthread_cond_t rx_cond;

	/* socket handle to HDCP daemon */
	int fd;
};

/* VBS-U virtio_ops */
static void virtio_hdcp_reset(void *);

static struct virtio_ops virtio_hdcp_ops = {
	"virtio_hdcp",			/* our name */
	VIRTIO_HDCP_NUMQ,		/* we support one virtqueue */
	0,				/* config reg size */
	virtio_hdcp_reset,		/* reset */
	NULL,				/* device-wide qnotify */
	NULL,				/* read virtio config */
	NULL,				/* write virtio config */
	NULL,				/* apply negotiated features */
	NULL,				/* called on guest set status */
};

/* Debug printf */
static int virtio_hdcp_debug;
#define DPRINTF(params) do { if (virtio_hdcp_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

static void
virtio_hdcp_reset(void *vdev)
{
	struct virtio_hdcp *vhdcp = vdev;

	DPRINTF(("virtio_hdcp: device reset requested\n"));
	virtio_reset_dev(&vhdcp->base);
}

static int
SendMessage(int fd, uint8_t* data, int dataSz)
{
	int bytesRemaining = dataSz;
	int offset = 0;

	while (bytesRemaining > 0)
	{
		ssize_t count = send(fd, &data[offset], bytesRemaining, MSG_NOSIGNAL);
		if (-1 == count)
		{
			if ((EINTR == errno) || (EAGAIN == errno))
			{
				continue;
			}

			WPRINTF(("Failed to send!"));
			return errno;
		}

		bytesRemaining -= count;
		offset += count;
	}
	return 0;
}

static int
GetMessage(int fd, uint8_t* data, int dataSz)
{
	int count = 0;
	int bytesRemaining = dataSz;
	int offset = 0;

	while (bytesRemaining > 0)
	{
		count = read(fd, &data[offset], bytesRemaining);
		if (-1 == count)
		{
			if ((EINTR == errno) || (EAGAIN == errno))
			{
				continue;
			}

			WPRINTF(("Failed to read!"));
			return errno;
		}

		if (0 == count)
		{
			WPRINTF(("Success to read, but the content is empty!"));
			return ENOTCONN;
		}

		bytesRemaining -= count;
		offset += count;
	}
	return 0;
}

static int
performMessageTransaction(int fd, struct SocketData data)
{
	int ret;
	// Send the request to daemon
	ret = SendMessage(fd, &data.Bytes, sizeof(data));
	if (ret < 0) {
		WPRINTF(("send error\n"));
		return ret;
	}

	// Get reply from daemon
	ret = GetMessage(fd, &data.Bytes, sizeof(data));
	if (ret < 0) {
		WPRINTF(("recv error\n"));
		return ret;
	}

	return 0;
}

static void *
virtio_hdcp_talk_to_daemon(void *param)
{
	struct virtio_hdcp *vhdcp = param;
	struct virtio_vq_info *rvq = &vhdcp->queues[0];
	struct iovec iov;
	uint16_t idx;
	int ret;
	struct SocketData *msg;

	for (;;) {
		pthread_mutex_lock(&vhdcp->rx_mtx);
		vhdcp->in_progress = 0;

		/*
		 * Checking the avail ring here serves two purposes:
		 *  - avoid vring processing due to spurious wakeups
		 *  - catch missing notifications before acquiring rx_mtx
		 */
		while (!vq_has_descs(rvq))
			pthread_cond_wait(&vhdcp->rx_cond, &vhdcp->rx_mtx);

		vhdcp->in_progress = 1;
		pthread_mutex_unlock(&vhdcp->rx_mtx);

		do {
			ret = vq_getchain(rvq, &idx, &iov, 1, NULL);
			if (ret < 1) {
				pr_err("%s: fail to getchain!\n", __func__);
				return NULL;
			}
			if (ret > 1) {
				pr_warn("%s: invalid chain!\n", __func__);
				vq_relchain(rvq, idx, 0);
				continue;
			}

			msg = (struct SocketData*)(iov.iov_base);

			ret = performMessageTransaction(vhdcp->fd, *msg);
			if (ret < 0) {
				close(vhdcp->fd);
				vhdcp->fd = -1;
			}

			/* release this chain and handle more */
			vq_relchain(rvq, idx, sizeof(struct SocketData));
		} while (vq_has_descs(rvq));

		/* at least one avail ring element has been processed */
		vq_endchains(rvq, 1);
	}
}

static int
connect_hdcp_daemon()
{
	struct sockaddr_un addr;
	int fd;
	int ret;

	fd = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (fd < 0) {
		WPRINTF(("socket error %d\n", errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, HDCP_SDK_SOCKET_PATH, sizeof(addr.sun_path));

	ret = connect(fd, &addr, sizeof(struct sockaddr_un));
	if (ret < 0) {
		WPRINTF(("connect error %d\n", errno));
		close(fd);
		return -1;
	}
	return fd;
}

static void
virtio_hdcp_notify(void *vdev, struct virtio_vq_info *vq)
{
	struct virtio_hdcp *vhdcp = vdev;

	/* Any ring entries to process */
	if (!vq_has_descs(vq))
		return;

	vhdcp->fd = (vhdcp->fd < 0) ? connect_hdcp_daemon() : vhdcp->fd;
	if (vhdcp->fd < 0)
	{
		WPRINTF(("Invalid HDCP daemon file descriptor\n"));
		return;
	}

	/* Signal the thread for processing */
	pthread_mutex_lock(&vhdcp->rx_mtx);
	if (vhdcp->in_progress == 0)
		pthread_cond_signal(&vhdcp->rx_cond);
	pthread_mutex_unlock(&vhdcp->rx_mtx);
}

static int
virtio_hdcp_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_hdcp *vhdcp;

	pthread_mutexattr_t attr;
	char tname[MAXCOMLEN + 1];
	int rc;

	vhdcp = calloc(1, sizeof(struct virtio_hdcp));
	if (!vhdcp) {
		WPRINTF(("vhdcp init: fail to alloc virtio_hdcp\n"));
		return -1;
	}

	/* init mutex attribute properly */
	int mutexattr_type = virtio_uses_msix()
			? PTHREAD_MUTEX_DEFAULT
			: PTHREAD_MUTEX_RECURSIVE;

	rc = pthread_mutexattr_init(&attr);
	if (rc)
		WPRINTF(("vhdcp init: mutexattr init fail, erro %d\n", rc));
	rc = pthread_mutexattr_settype(&attr, mutexattr_type);
	if (rc)
		WPRINTF(("vhdcp init: mutexattr_settype fail, erro %d\n", rc));
	rc = pthread_mutex_init(&vhdcp->mtx, &attr);
	if (rc)
		WPRINTF(("vhdcp init: mutexattr_settype fail, erro %d\n", rc));

	DPRINTF(("vhdcp init: using VBS-U...\n"));
	virtio_linkup(&vhdcp->base, &virtio_hdcp_ops,
			vhdcp, dev, vhdcp->queues, BACKEND_VBSU);
	vhdcp->base.mtx = &vhdcp->mtx;

	vhdcp->queues[0].qsize  = VIRTIO_HDCP_RINGSZ;
	vhdcp->queues[0].notify = virtio_hdcp_notify;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_HDCP);
	pci_set_cfgdata16(dev, PCIR_VENDOR, INTEL_VENDOR_ID);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_CRYPTO);
	pci_set_cfgdata8(dev, PCIR_SUBCLASS, PCIS_SIMPLECOMM_OTHER);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_HDCP);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, INTEL_VENDOR_ID);

	if (virtio_interrupt_init(&vhdcp->base, virtio_uses_msix())) {
		WPRINTF(("vhdcp init: interrupt init fail\n"));
		free(vhdcp);
		return -1;
	}

	virtio_set_io_bar(&vhdcp->base, 0);

	/*
	 * connect to hdcp daemon in init phase
	 *
	 * @FIXME if failed connecting to HDCP daemon, the return value should
	 * be set appropriately for SOS not exposing the HDCP PCI device to UOS
	 */
	vhdcp->fd = connect_hdcp_daemon();
	if (vhdcp->fd < 0) {
		WPRINTF(("connection to server failed\n"));
		return -errno;
	}

	vhdcp->in_progress = 0;
	pthread_mutex_init(&vhdcp->rx_mtx, NULL);
	pthread_cond_init(&vhdcp->rx_cond, NULL);
	pthread_create(&vhdcp->rx_tid, NULL,
			virtio_hdcp_talk_to_daemon, (void *)vhdcp);
	snprintf(tname, sizeof(tname), "vthdcp-%d:%d tx",
			dev->slot, dev->func);
	pthread_setname_np(vhdcp->rx_tid, tname);

	return 0;
}

static void
virtio_hdcp_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_hdcp *vhdcp = (struct virtio_hdcp *)dev->arg;

	if (vhdcp) {
		DPRINTF(("free struct virtio_hdcp\n"));
		free(vhdcp);
	}
}

struct pci_vdev_ops pci_ops_virtio_hdcp = {
	.class_name		= "virtio-hdcp",
	.vdev_init		= virtio_hdcp_init,
	.vdev_deinit		= virtio_hdcp_deinit,
	.vdev_barwrite		= virtio_pci_write,
	.vdev_barread		= virtio_pci_read
};

DEFINE_PCI_DEVTYPE(pci_ops_virtio_hdcp);
