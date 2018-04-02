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
/*
 * HECI device virtualization.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <linux/mei.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "dm.h"
#include "mevent.h"
#include "pci_core.h"
#include "virtio.h"
#include "heci.h"

#define VIRTIO_HECI_RXQ		0
#define VIRTIO_HECI_TXQ		1
#define VIRTIO_HECI_RXSEGS	1
#define VIRTIO_HECI_TXSEGS	2

/*
 * HECI HW max support FIFO depth is 128
 * We might support larger depth, which need change MEI driver
 */
#define VIRTIO_HECI_FIFOSZ	128

#define VIRTIO_HECI_RINGSZ	64
#define VIRTIO_HECI_VQNUM	2

#define HECI_NATIVE_DEVICE_NODE	"/dev/mei0"

/*
 * Different type has differnt emulation
 *
 * TYPE_HBM: to emulate HBM commands' request and response
 * TYPE_NORAML: to emulate ME clients request and response
 */
enum heci_client_type {
	TYPE_NORMAL,
	TYPE_HBM,

	TYPE_MAX
};

struct virtio_heci_config {
	int	buf_depth;
	uint8_t	hw_ready;
	uint8_t	host_reset;
} __attribute__((packed));

/*
 * abstract virtio_heci_client for ME client and host client mapping
 * client_fd   - SOS kernel mei_me_client
 * client_addr - UOS kernel FE driver mei_me_client
 * client_id   - native ME client's client_id, used acrossing native, BE and FE
 * props       - ME client's properties
 */
struct virtio_heci_client {
	struct virtio_heci		*vheci;
	LIST_ENTRY(virtio_heci_client)	list;
	uuid_t				client_uuid;
	int				client_fd;
	int				client_addr;
	uint8_t				client_id;
	enum heci_client_type		type;
	struct heci_client_properties	props;

	struct mevent			*rx_mevp;
	uint8_t				*recv_buf;
	int				recv_buf_sz;
	int				recv_offset;
	int				recv_handled;
	int				recv_creds;

	uint8_t				*send_buf;
	int				send_buf_sz;
	int				send_idx;

	int				ref;
};

struct virtio_heci {
	struct virtio_base		base;
	struct virtio_vq_info		vqs[VIRTIO_HECI_VQNUM];
	pthread_mutex_t			mutex;
	struct mei_enumerate_me_clients	me_clients_map;

	struct virtio_heci_config	*config;

	pthread_mutex_t			list_mutex;
	LIST_HEAD(listhead, virtio_heci_client) active_clients;
	struct virtio_heci_client	*hbm_client;
};

/*
 * Debug printf
 */
static int virtio_heci_debug;
#define DPRINTF(params) do { if (virtio_heci_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

static void virtio_heci_reset(void *);
static void virtio_heci_rx_callback(int fd, enum ev_type type, void *param);
static void virtio_heci_notify_rx(void *, struct virtio_vq_info *);
static void virtio_heci_notify_tx(void *, struct virtio_vq_info *);
static int virtio_heci_cfgread(void *, int, int, uint32_t *);
static int virtio_heci_cfgwrite(void *, int, int, uint32_t);
static struct virtio_heci_client *virtio_heci_create_client(
		struct virtio_heci *vheci,
		int client_id, int client_addr, int *status);
static void virtio_heci_destroy_client(struct virtio_heci *vheci,
		struct virtio_heci_client *client);

static struct virtio_ops virtio_heci_ops = {
	"virtio_heci",		/* our name */
	VIRTIO_HECI_VQNUM,	/* we support several virtqueues */
	sizeof(struct virtio_heci_config), /* config reg size */
	virtio_heci_reset,	/* reset */
	NULL,			/* device-wide qnotify */
	virtio_heci_cfgread,	/* read virtio config */
	virtio_heci_cfgwrite,	/* write virtio config */
	NULL,			/* apply negotiated features */
	NULL,                   /* called on guest set status */
	0,			/* our capabilities */
};

static void
print_hex(uint8_t *bytes, int length)
{
	int i;

	if (!virtio_heci_debug)
		return;

	for (i = 0; i < length; i++) {
		if (i % 16 == 0 && i != 0)
			printf("\r\n");
		printf("%02x ", bytes[i]);
	}
	printf("\r\n");
}

/*
 * Convert client connect ioctl errno to client connection response code
 * we might lost some exact response code because the errno of ioctl is rough
 * However, FE driver will get same errno for UOS's userspace.
 */
static inline int
err_to_native_heci_resno(int err)
{
	switch (err) {
	case 0:		return HECI_HBM_SUCCESS;
	case -ENOTTY:	return HECI_HBM_CLIENT_NOT_FOUND;
	case -EBUSY:	return HECI_HBM_ALREADY_STARTED;
	case -EINVAL:	return HECI_HBM_INVALID_PARAMETER;
	default:	return HECI_HBM_INVALID_PARAMETER;
	}
}

static struct virtio_heci_client *
virtio_heci_client_get(struct virtio_heci_client *client)
{
	if (__sync_fetch_and_add(&client->ref, 1) == 0)
		return NULL;
	return client;
}

static void
virtio_heci_client_put(struct virtio_heci *vheci,
		struct virtio_heci_client *client)
{
	assert(client->ref > 0);
	if (__sync_sub_and_fetch(&client->ref, 1) == 0)
		virtio_heci_destroy_client(vheci, client);
}

static void
virtio_heci_add_client(struct virtio_heci *vheci,
		struct virtio_heci_client *client)
{
	pthread_mutex_lock(&vheci->list_mutex);
	LIST_INSERT_HEAD(&vheci->active_clients, client, list);
	pthread_mutex_unlock(&vheci->list_mutex);
}

static void
virtio_heci_del_client(struct virtio_heci *vheci,
		struct virtio_heci_client *client)
{
	pthread_mutex_lock(&vheci->list_mutex);
	LIST_REMOVE(client, list);
	pthread_mutex_unlock(&vheci->list_mutex);
}

static struct virtio_heci_client *
virtio_heci_create_client(struct virtio_heci *vheci, int client_id,
		int client_addr, int *status)
{
	struct virtio_heci_client *client;
	struct mei_connect_client_data connect;
	struct mei_request_client_params params_req;
	struct heci_client_properties *props;
	int ret = 0;
	int size = VIRTIO_HECI_FIFOSZ * sizeof(struct heci_msg_hdr);

	client = calloc(1, sizeof(struct virtio_heci_client));
	if (!client)
		return client;
	client->vheci = vheci;
	client->client_id = client_id;
	client->client_addr = client_addr;
	/* open mei node */
	client->client_fd = open(HECI_NATIVE_DEVICE_NODE, O_RDWR);
	if (client->client_fd < 0) {
		DPRINTF(("vheci: create_client: open mei failed!\r\n"));
		goto open_fail;
	}

	if (client_id == 0 && client_addr == 0) {
		/* TYPE_HBM */
		/* HBM don't need get prop
		 * but we get clients_map here
		 */
		ret = ioctl(client->client_fd, IOCTL_MEI_ENUMERATE_ME_CLIENTS,
				&vheci->me_clients_map);
		if (ret < 0)
			goto connect_fail;
		print_hex((uint8_t *)&vheci->me_clients_map,
				sizeof(vheci->me_clients_map));

		/* setup send_buf and recv_buf for client 0 */
		client->send_buf = calloc(1, size);
		client->recv_buf = calloc(1, size);
		if (!client->send_buf || !client->recv_buf) {
			DPRINTF(("vheci: hbm bufs calloc fail!\r\n"));
			goto alloc_buf_fail;
		}
		client->recv_buf_sz = size;
		client->send_buf_sz = size;
		client->type = TYPE_HBM;
	} else {
		/* get this client's props */
		params_req.client_id = client_id;
		ret = ioctl(client->client_fd,
				IOCTL_MEI_REQUEST_CLIENT_PROP, &params_req);
		if (ret < 0)
			goto connect_fail;
		props = (struct heci_client_properties *)params_req.data;
		memcpy(&client->props, props, sizeof(client->props));

		/* TYPE_NORMAL */
		/* connect to ME client */
		memcpy(&connect, &props->protocol_name,
				sizeof(props->protocol_name));
		ret = ioctl(client->client_fd,
				IOCTL_MEI_CONNECT_CLIENT, &connect);

		/* connecting might fail. */
		if (ret) {
			*status = err_to_native_heci_resno(ret);
			DPRINTF(("vheci: client connect fail!\r\n"));
			goto connect_fail;
		}

		size = client->props.max_msg_length;

		/*
		 * setup send_buf and recv_buf,
		 * need before adding client fd to mevent
		 */
		client->send_buf = calloc(1, size);
		client->recv_buf = calloc(1, size);
		if (!client->send_buf || !client->recv_buf) {
			DPRINTF(("vheci: client bufs calloc failed!\r\n"));
			goto alloc_buf_fail;
		}
		client->recv_buf_sz = size;
		client->send_buf_sz = size;
		client->type = TYPE_NORMAL;

		/* add READ event into mevent */
		client->rx_mevp = mevent_add(client->client_fd, EVF_READ,
				virtio_heci_rx_callback, client);
	}

	DPRINTF(("vheci: NEW client mapping: ME[%d]fd[%d]:FE[%d] "
			"protocol_ver[%d]|max_connections[%d]|fixed_addr[%d]|"
			"single_recv_buf[%d]|max_msg_len[%d]\n\r",
			client_id, client->client_fd, client_addr,
			client->props.protocol_version,
			client->props.max_connections,
			client->props.fixed_address,
			client->props.single_recv_buf,
			client->props.max_msg_length));

	client->ref = 1;
	virtio_heci_add_client(vheci, client);
	return client;

alloc_buf_fail:
	free(client->send_buf);
	free(client->recv_buf);
connect_fail:
	close(client->client_fd);
open_fail:
	free(client);
	return NULL;
}

static void
virtio_heci_destroy_client(struct virtio_heci *vheci,
		struct virtio_heci_client *client)
{
	if (!client || !vheci)
		return;
	DPRINTF(("vheci: Destroy client mapping: ME[%d]fd[%d]:FE[%d]\r\n",
				client->client_id, client->client_fd,
				client->client_addr));
	virtio_heci_del_client(vheci, client);
	if (client->type != TYPE_HBM)
		mevent_delete_close(client->rx_mevp);
	free(client->send_buf);
	free(client->recv_buf);
	free(client);
}

/*
 * A completed read guarantees that a client message is completed,
 * transmission of client message is started by a flow control message of HBM
 */
static int
native_heci_read(struct virtio_heci_client *client)
{
	int len;

	if (client->client_fd <= 0) {
		DPRINTF(("vheci: write failed! invalid client_fd[%d]\r\n",
					client->client_fd));
		return -1;
	}

	/* read with max_message_length */
	len = read(client->client_fd, client->recv_buf, client->recv_buf_sz);
	if (len < 0) {
		WPRINTF(("vheci: read failed! read error[%d]\r\n", errno));
		return len;
	}
	DPRINTF(("vheci: RX: ME[%d]fd[%d] Append data(len[%d]) "
			       "at recv_buf(off[%d]).\n\r",
				client->client_id, client->client_fd,
				len, client->recv_offset));

	client->recv_offset = len;
	return client->recv_offset;
}

static void
virtio_heci_rx_callback(int fd, enum ev_type type, void *param)
{
	struct virtio_heci_client *client = param;
	struct virtio_heci *vheci = client->vheci;
	int ret;

	if (!virtio_heci_client_get(client)) {
		DPRINTF(("vheci: client has been released, ignore data.\r\n"));
		return;
	}

	/* read data from mei driver */
	ret = native_heci_read(client);
	if (ret < 0)
		/* read failed, no data available */
		goto out;

	DPRINTF(("vheci: RX: ME[%d]fd[%d] read %d bytes from MEI.\r\n",
				client->client_id, fd, ret));

out:
	virtio_heci_client_put(vheci, client);
}

static void
virtio_heci_reset(void *vth)
{
	struct virtio_heci *vheci = vth;

	DPRINTF(("vheci: device reset requested !\r\n"));
	virtio_reset_dev(&vheci->base);
}

static void
virtio_heci_notify_rx(void *heci, struct virtio_vq_info *vq)
{
	/*
	 * Any ring entries to process?
	 */
	if (!vq_has_descs(vq))
		return;
}

static void
virtio_heci_notify_tx(void *heci, struct virtio_vq_info *vq)
{
	/*
	 * Any ring entries to process?
	 */
	if (!vq_has_descs(vq))
		return;
}

static int
virtio_heci_cfgread(void *vsc, int offset, int size, uint32_t *retval)
{
	struct virtio_heci *vheci = vsc;
	void *ptr;

	ptr = (uint8_t *)vheci->config + offset;
	memcpy(retval, ptr, size);
	return 0;
}

static int
virtio_heci_cfgwrite(void *vsc, int offset, int size, uint32_t val)
{
	/* TODO: need handle device config writing */
	return 0;
}

static int
virtio_heci_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_heci *vheci;
	pthread_mutexattr_t attr;
	int i, rc, status = 0;

	vheci = calloc(1, sizeof(struct virtio_heci));
	if (!vheci) {
		WPRINTF(("vheci init: fail to alloc virtio_heci!\r\n"));
		return -1;
	}
	vheci->config = calloc(1, sizeof(struct virtio_heci_config));
	if (!vheci->config) {
		WPRINTF(("vheci init: fail to alloc virtio_heci_config!\r\n"));
		goto fail;
	}
	vheci->config->buf_depth = VIRTIO_HECI_FIFOSZ;
	vheci->config->hw_ready = 1;

	/* init mutex attribute properly */
	rc = pthread_mutexattr_init(&attr);
	if (rc) {
		WPRINTF(("vheci init: mutexattr init fail, erro %d!\r\n", rc));
		goto fail;
	}
	if (fbsdrun_virtio_msix()) {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
		if (rc) {
			WPRINTF(("vheci init: mutexattr_settype failed with "
				"error %d!\r\n", rc));
			goto fail;
		}
	} else {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		if (rc) {
			WPRINTF(("vheci init: mutexattr_settype failed with "
				"error %d!\r\n", rc));
			goto fail;
		}
	}
	rc = pthread_mutex_init(&vheci->mutex, &attr);
	if (rc) {
		WPRINTF(("vheci init: mutex init failed, error %d!\r\n", rc));
		goto fail;
	}

	virtio_linkup(&vheci->base, &virtio_heci_ops,
			vheci, dev, vheci->vqs);
	vheci->base.mtx = &vheci->mutex;

	for (i = 0; i < VIRTIO_HECI_VQNUM; i++)
		vheci->vqs[i].qsize = VIRTIO_HECI_RINGSZ;
	vheci->vqs[VIRTIO_HECI_RXQ].notify = virtio_heci_notify_rx;
	vheci->vqs[VIRTIO_HECI_TXQ].notify = virtio_heci_notify_tx;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_HECI);
	pci_set_cfgdata16(dev, PCIR_VENDOR, INTEL_VENDOR_ID);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_SIMPLECOMM);
	pci_set_cfgdata8(dev, PCIR_SUBCLASS, PCIS_SIMPLECOMM_OTHER);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_HECI);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, INTEL_VENDOR_ID);

	if (virtio_interrupt_init(&vheci->base, fbsdrun_virtio_msix()))
		goto setup_fail;
	virtio_set_io_bar(&vheci->base, 0);

	/*
	 * init clients
	 */
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&vheci->list_mutex, &attr);
	LIST_INIT(&vheci->active_clients);

	/*
	 * create a client for HBM
	 */
	vheci->hbm_client = virtio_heci_create_client(vheci, 0, 0, &status);
	if (!vheci->hbm_client)
		goto fail;

	return 0;
setup_fail:
	pthread_mutex_destroy(&vheci->mutex);
fail:
	free(vheci->config);
	free(vheci);
	return -1;
}

static void
virtio_heci_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_heci *vheci = (struct virtio_heci *)dev->arg;

	virtio_heci_client_put(vheci, vheci->hbm_client);
	pthread_mutex_destroy(&vheci->list_mutex);
	pthread_mutex_destroy(&vheci->mutex);
	free(vheci->config);
	free(vheci);
}

struct pci_vdev_ops pci_ops_vheci = {
	.class_name	= "virtio-heci",
	.vdev_init	= virtio_heci_init,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read,
	.vdev_deinit    = virtio_heci_deinit
};
DEFINE_PCI_DEVTYPE(pci_ops_vheci);
