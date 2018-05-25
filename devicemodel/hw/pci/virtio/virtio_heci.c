/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
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
#include <stddef.h>
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

enum vheci_status {
	VHECI_READY,
	VHECI_PENDING_RESET,
	VHECI_RESET,
	VHECI_DEINIT
};

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
	volatile enum vheci_status	status;

	pthread_t			tx_thread;
	pthread_mutex_t			tx_mutex;
	pthread_cond_t			tx_cond;

	pthread_t			rx_thread;
	pthread_mutex_t			rx_mutex;
	pthread_cond_t			rx_cond;
	bool				rx_need_sched;

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
static void virtio_heci_virtual_fw_reset(struct virtio_heci *vheci);
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
	int new, val;

	/*
	 * The active_clients be protected by list_mutex
	 * so the client never be null
	 */
	do {
		val = *(volatile int *)&client->ref;
		if (val == 0)
			return NULL;

		new = val + 1;

		/* check for overflow */
		assert(new > 0);

	} while (!__sync_bool_compare_and_swap(&client->ref, val, new));
	return client;
}

static void
virtio_heci_client_put(struct virtio_heci *vheci,
		struct virtio_heci_client *client)
{
	int new, val;

	do {
		val = *(volatile int *)&client->ref;
		if (val == 0)
			return;

		new = val - 1;

	} while (!__sync_bool_compare_and_swap(&client->ref, val, new));

	if (client->ref == 0)
		virtio_heci_destroy_client(vheci, client);
}

static void
virtio_heci_add_client(struct virtio_heci *vheci,
		struct virtio_heci_client *client)
{
	pthread_mutex_lock(&vheci->list_mutex);
	if (client->type == TYPE_HBM)
		/* make sure hbm client at the head of the list */
		LIST_INSERT_HEAD(&vheci->active_clients, client, list);
	else
		LIST_INSERT_AFTER(LIST_FIRST(&vheci->active_clients),
				client, list);
	pthread_mutex_unlock(&vheci->list_mutex);
}

static inline void
virtio_heci_set_status(struct virtio_heci *vheci, enum vheci_status status)
{
	if (status == VHECI_DEINIT ||
		status == VHECI_READY ||
		status > vheci->status)
		vheci->status = status;
}

static struct virtio_heci_client *
virtio_heci_find_client(struct virtio_heci *vheci, int client_addr)
{
	struct virtio_heci_client *pclient, *client = NULL;

	pthread_mutex_lock(&vheci->list_mutex);
	LIST_FOREACH(pclient, &vheci->active_clients, list) {
		if (pclient->client_addr == client_addr) {
			client = virtio_heci_client_get(pclient);
			break;
		}
	}
	pthread_mutex_unlock(&vheci->list_mutex);
	return client;
}

static struct virtio_heci_client *
virtio_heci_get_hbm_client(struct virtio_heci *vheci)
{
	struct virtio_heci_client *client = NULL;

	pthread_mutex_lock(&vheci->list_mutex);
	if (vheci->hbm_client)
		client = virtio_heci_client_get(vheci->hbm_client);
	pthread_mutex_unlock(&vheci->list_mutex);
	return client;
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
	struct virtio_heci *vheci = client->vheci;
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
		if (errno == ENODEV)
			virtio_heci_set_status(vheci, VHECI_PENDING_RESET);
		return len;
	}
	DPRINTF(("vheci: RX: ME[%d]fd[%d] Append data(len[%d]) "
			       "at recv_buf(off[%d]).\n\r",
				client->client_id, client->client_fd,
				len, client->recv_offset));

	client->recv_offset = len;
	return client->recv_offset;
}

static int
native_heci_write(struct virtio_heci_client *client)
{
	struct virtio_heci *vheci = client->vheci;
	int len;

	if (client->client_fd <= 0) {
		DPRINTF(("vheci: write failed! invalid client_fd[%d]\r\n",
					client->client_fd));
		return -1;
	}
	len = write(client->client_fd, client->send_buf, client->send_idx);
	if (len < 0) {
		WPRINTF(("vheci: write failed! write error[%d]\r\n", errno));
		if (errno == ENODEV)
			virtio_heci_set_status(vheci, VHECI_PENDING_RESET);
	}
	return len;
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

	pthread_mutex_lock(&vheci->rx_mutex);
	if (client->recv_offset != 0 || vheci->status != VHECI_READY) {
		/* still has data in recv_buf, wait guest reading */
		goto out;
	}

	/* read data from mei driver */
	ret = native_heci_read(client);
	if (ret > 0)
		vheci->rx_need_sched = true;
	else
		/* read failed, no data available */
		goto out;

	DPRINTF(("vheci: RX: ME[%d]fd[%d] read %d bytes from MEI.\r\n",
				client->client_id, fd, ret));

	// wake up rx thread.
	pthread_cond_signal(&vheci->rx_cond);
out:
	pthread_mutex_unlock(&vheci->rx_mutex);
	virtio_heci_client_put(vheci, client);
	if (vheci->status == VHECI_PENDING_RESET) {
		virtio_heci_virtual_fw_reset(vheci);
		/* Let's wait 100ms for HBM enumeration done */
		usleep(100000);
		virtio_config_changed(&vheci->base);
	}

}

static void
virtio_heci_reset(void *vth)
{
	struct virtio_heci *vheci = vth;

	DPRINTF(("vheci: device reset requested !\r\n"));
	virtio_heci_virtual_fw_reset(vheci);
	virtio_reset_dev(&vheci->base);
}

static inline bool
hdr_is_hbm(struct heci_msg_hdr *heci_hdr)
{
	return heci_hdr->host_addr == 0 && heci_hdr->me_addr == 0;
}

static inline bool
hdr_is_fixed(struct heci_msg_hdr *heci_hdr)
{
	return heci_hdr->host_addr == 0 && heci_hdr->me_addr != 0;
}

static void
populate_heci_hdr(struct virtio_heci_client *client,
		struct heci_msg_hdr *hdr, int len, int complete)
{
	assert(hdr != NULL);
	memset(hdr, 0, sizeof(*hdr));
	hdr->me_addr = client->client_id;
	hdr->host_addr = client->client_addr;
	hdr->length = len;
	hdr->msg_complete = complete;
}

static void
virtio_heci_hbm_response(struct virtio_heci *vheci,
		struct heci_msg_hdr *hdr, void *data, int len)
{
	struct virtio_heci_client *client;

	client = virtio_heci_get_hbm_client(vheci);
	if (!client) {
		DPRINTF(("vheci: HBM client has been released, ignore.\r\n"));
		return;
	}
	pthread_mutex_lock(&vheci->rx_mutex);
	memcpy(client->recv_buf + client->recv_offset, hdr,
			sizeof(struct heci_msg_hdr));
	client->recv_offset += sizeof(struct heci_msg_hdr);
	memcpy(client->recv_buf + client->recv_offset, data, len);
	client->recv_offset += len;
	vheci->rx_need_sched = true;

	pthread_mutex_unlock(&vheci->rx_mutex);
	virtio_heci_client_put(vheci, client);
}

/*
 * This function is intend to send HBM disconnection command to guest.
 * Although the default name is for virtual, using vclient here for
 * better understanding.
 */
static void
virtio_heci_disconnect_vclient(struct virtio_heci *vheci,
		struct virtio_heci_client *client)
{
	struct heci_msg_hdr hdr = {0};
	struct heci_hbm_client_disconnect_req disconnect_req = {{0} };
	struct virtio_heci_client *hbm_client;

	disconnect_req.hbm_cmd.cmd = HECI_HBM_CLIENT_DISCONNECT;
	disconnect_req.me_addr = client->client_id;
	disconnect_req.host_addr = client->client_addr;

	hbm_client = virtio_heci_get_hbm_client(vheci);
	if (!hbm_client) {
		DPRINTF(("vheci: HBM client get fail!!\r\n"));
		return;
	}
	populate_heci_hdr(hbm_client, &hdr,
			sizeof(struct heci_hbm_client_disconnect_req), 1);

	DPRINTF(("vheci: DM -> UOS disconnect client[%d]!\r\n",
				client->client_id));
	virtio_heci_hbm_response(vheci, &hdr,
			&disconnect_req, sizeof(disconnect_req));
	virtio_heci_client_put(vheci, hbm_client);
}

static void
virtio_heci_hbm_handler(struct virtio_heci *vheci, void *data)
{
	struct heci_hbm_cmd *hbm_cmd = (struct heci_hbm_cmd *)data;
	struct heci_msg_hdr *phdr, hdr = {0};
	struct virtio_heci_client *client = NULL;
	struct mei_request_client_params params_req = {0};
	int ret = 0, status = 0;

	struct heci_hbm_host_ver_res ver_res = {{0} };
	struct heci_hbm_host_enum_req *enum_req;
	struct heci_hbm_host_enum_res enum_res = {{0} };
	struct heci_hbm_host_client_prop_req *client_prop_req;
	struct heci_hbm_host_client_prop_res client_prop_res = {{0} };
	struct heci_hbm_client_connect_req *connect_req;
	struct heci_hbm_client_connect_res connect_res = {{0} };
	struct heci_hbm_client_disconnect_res disconnect_res = {{0} };
	struct heci_hbm_flow_ctl flow_ctl_req = {{0} };
	struct heci_hbm_flow_ctl *pflow_ctl_req;

	DPRINTF(("vheci: HBM cmd[%d] is handling\n\r",	hbm_cmd->cmd));
	phdr = &hdr;
	switch (hbm_cmd->cmd) {
	case HECI_HBM_HOST_VERSION:
		ver_res = *(struct heci_hbm_host_ver_res *)data;
		ver_res.hbm_cmd.is_response = 1;
		/* always support host request version */
		ver_res.host_ver_support = 1;
		populate_heci_hdr(vheci->hbm_client, phdr,
				sizeof(struct heci_hbm_host_ver_res), 1);
		DPRINTF(("vheci: HBM cmd[%d] response: hdr[%08x]\n\r",
					hbm_cmd->cmd, *(unsigned int *)phdr));
		print_hex((unsigned char *)&ver_res, sizeof(ver_res));
		virtio_heci_hbm_response(vheci, phdr,
				&ver_res, sizeof(ver_res));
		break;
	case HECI_HBM_HOST_STOP:
		/* TODO:
		 * disconnect all clients
		 */
		DPRINTF(("vheci: %s HBM cmd[%d] not support for now\n\r",
					__func__, hbm_cmd->cmd));
		break;
	case HECI_HBM_ME_STOP:
		/* TODO:
		 * reset all device
		 */
		DPRINTF(("vheci: %s HBM cmd[%d] not support for now\n\r",
					__func__, hbm_cmd->cmd));
		break;
	case HECI_HBM_HOST_ENUM:
		enum_req = (struct heci_hbm_host_enum_req *)data;
		enum_res.hbm_cmd = enum_req->hbm_cmd;
		enum_res.hbm_cmd.is_response = 1;
		/* copy valid_addresses for hbm enum request */
		memcpy(enum_res.valid_addresses, &vheci->me_clients_map,
				sizeof(enum_res.valid_addresses));
		populate_heci_hdr(vheci->hbm_client, phdr,
				sizeof(struct heci_hbm_host_enum_res), 1);
		DPRINTF(("vheci: HBM cmd[%d] response: hdr[%08x]\n\r",
					hbm_cmd->cmd, *(unsigned int *)phdr));
		print_hex((unsigned char *)&enum_res, sizeof(enum_res));
		virtio_heci_hbm_response(vheci, phdr,
				&enum_res, sizeof(enum_res));
		break;
	case HECI_HBM_HOST_CLIENT_PROP:
		client_prop_req = (struct heci_hbm_host_client_prop_req *)data;
		client_prop_res.hbm_cmd = client_prop_req->hbm_cmd;
		client_prop_res.hbm_cmd.is_response = 1;

		client_prop_res.status = HECI_HBM_SUCCESS;
		/* get the client's props */
		client_prop_res.address = params_req.client_id =
			client_prop_req->address;
		ret = ioctl(vheci->hbm_client->client_fd,
				IOCTL_MEI_REQUEST_CLIENT_PROP, &params_req);
		if (ret < 0) {
			/* not client found */
			client_prop_res.status = HECI_HBM_CLIENT_NOT_FOUND;
		}
		memcpy(&client_prop_res.props, params_req.data,
				sizeof(struct heci_client_properties));
		populate_heci_hdr(vheci->hbm_client, phdr,
			sizeof(struct heci_hbm_host_client_prop_res), 1);
		DPRINTF(("vheci: HBM cmd[%d] response: hdr[%08x]\n\r",
					hbm_cmd->cmd, *(unsigned int *)phdr));
		print_hex((unsigned char *)&client_prop_res,
				sizeof(client_prop_res));
		virtio_heci_hbm_response(vheci, phdr,
				&client_prop_res, sizeof(client_prop_res));
		break;
	case HECI_HBM_CLIENT_CONNECT:
		/*
		 * need handle some driver probed clients.
		 * There are some single connection clients with kernel
		 * driver probed, like DAL's VM clients, we need emulate
		 * the response to guest and setup the link between
		 * virtio_heci_client and front-end client
		 */
		connect_req = (struct heci_hbm_client_connect_req *)data;
		connect_res =
			*(struct heci_hbm_client_connect_res *)connect_req;
		connect_res.hbm_cmd.is_response = 1;
		client = virtio_heci_find_client(vheci,	connect_req->host_addr);
		if (!client) {
			/* no client mapping in mediator, need create */
			client = virtio_heci_create_client(vheci,
					connect_req->me_addr,
					connect_req->host_addr, &status);
			if (!client) {
				/* create client failed
				 * TODO: need change the status according to
				 * the status of virtio_heci_create_client.
				 */
				connect_res.status = status;
			} else
				virtio_heci_client_get(client);
		} else {
			/* already exist, return @HECI_HBM_ALREADY_EXISTS */
			connect_res.status = HECI_HBM_ALREADY_EXISTS;
		}
		populate_heci_hdr(vheci->hbm_client, phdr,
				sizeof(struct heci_hbm_client_connect_res), 1);
		DPRINTF(("vheci: HBM cmd[%d] response: hdr[%08x]\n\r",
					hbm_cmd->cmd, *(unsigned int *)phdr));
		print_hex((unsigned char *)&connect_res, sizeof(connect_res));
		virtio_heci_hbm_response(vheci, phdr,
				&connect_res, sizeof(connect_res));

		/* create client failed, not need to send flow control */
		if (!client) {
			DPRINTF(("vheci: create client failed.\r\n"));
			break;
		}

		/* Give guest a flow control credit to let it send msssage */
		flow_ctl_req.hbm_cmd.cmd = HECI_HBM_FLOW_CONTROL;
		flow_ctl_req.me_addr = client->client_id;
		/*
		 * single recv buffer client,
		 */
		if (client->props.single_recv_buf)
			flow_ctl_req.host_addr = 0;
		else
			flow_ctl_req.host_addr = client->client_addr;

		virtio_heci_client_put(vheci, client);
		populate_heci_hdr(vheci->hbm_client, phdr,
				sizeof(struct heci_hbm_flow_ctl), 1);
		DPRINTF(("vheci: HBM flow control: hdr[%08x]\n\r",
				       *(unsigned int *)phdr));
		print_hex((unsigned char *)&flow_ctl_req, sizeof(flow_ctl_req));
		virtio_heci_hbm_response(vheci, phdr,
				&flow_ctl_req, sizeof(flow_ctl_req));
		break;
	case HECI_HBM_CLIENT_DISCONNECT:
		disconnect_res = *(struct heci_hbm_client_disconnect_res *)data;
		if (!hbm_cmd->is_response) {
			disconnect_res.hbm_cmd.is_response = 1;
			disconnect_res.status = 0;
			populate_heci_hdr(vheci->hbm_client, phdr,
				sizeof(struct heci_hbm_client_disconnect_res),
				1);
			virtio_heci_hbm_response(vheci, phdr,
				&disconnect_res, sizeof(disconnect_res));
		}
		client = virtio_heci_find_client(vheci,
				disconnect_res.host_addr);
		if (client) {
			virtio_heci_client_put(vheci, client);
			/* put once more as find_client will get ref */
			virtio_heci_client_put(vheci, client);
		} else
			DPRINTF(("vheci: client has been disconnected!!\r\n"));
		break;
	case HECI_HBM_FLOW_CONTROL:
		/*
		 * FE client is ready, we can send message
		 */
		pflow_ctl_req = (struct heci_hbm_flow_ctl *)data;
		client = virtio_heci_find_client(vheci,
				pflow_ctl_req->host_addr);
		if (client) {
			client->recv_creds++;
			virtio_heci_client_put(vheci, client);
			pthread_mutex_lock(&vheci->rx_mutex);
			vheci->rx_need_sched = true;
			pthread_mutex_unlock(&vheci->rx_mutex);
		} else
			DPRINTF(("vheci: client has been released.\r\n"));
		break;
	case HECI_HBM_CLIENT_CONNECTION_RESET:
		/* TODO:
		 * disconnect all clients
		 */
		DPRINTF(("vheci: %s HBM cmd[%d] not support for now\n\r",
					__func__, hbm_cmd->cmd));
		break;
	default:
		DPRINTF(("vheci: %s HBM cmd[%d] not support for now\n\r",
					__func__, hbm_cmd->cmd));
		break;
	}
	DPRINTF(("vheci: HBM cmd[%d] is done!\n\r", hbm_cmd->cmd));
}

static void
virtio_heci_proc_tx(struct virtio_heci *vheci, struct virtio_vq_info *vq)
{
	struct iovec iov[VIRTIO_HECI_TXSEGS + 1];
	struct heci_msg_hdr *heci_hdr;
	uint16_t idx;
	struct virtio_heci_client *client = NULL, *hbm_client;
	int tlen, len;
	int n;
	struct heci_msg_hdr hdr;
	struct heci_hbm_flow_ctl flow_ctl_req = {{0} };

	/*
	 * Obtain chain of descriptors.
	 * The first one is heci_hdr, the second is for payload.
	 */
	n = vq_getchain(vq, &idx, iov, VIRTIO_HECI_TXSEGS, NULL);
	assert(n == 2);
	heci_hdr = (struct heci_msg_hdr *)iov[0].iov_base;
	tlen = iov[0].iov_len + iov[1].iov_len;
	DPRINTF(("vheci: TX: UOS -> DM, hdr[%08x] length[%ld]\n\r",
				*(uint32_t *)heci_hdr, iov[1].iov_len));
	print_hex((uint8_t *)iov[1].iov_base, iov[1].iov_len);

	if (hdr_is_hbm(heci_hdr)) {
		/*
		 * hbm client handler
		 * all hbm will be complete in one msg package,
		 * so handle here directly
		 */
		hbm_client = virtio_heci_get_hbm_client(vheci);
		if (!hbm_client) {
			DPRINTF(("vheci: TX: HBM client get fail!!\r\n"));
			goto failed;
		}
		virtio_heci_hbm_handler(vheci, (void *)iov[1].iov_base);
		virtio_heci_client_put(vheci, hbm_client);
	} else if (hdr_is_fixed(heci_hdr)) {
		/*
		 * TODO: fixed address client handler
		 * fixed address client don't need receive, connectless
		 */
	} else {
		/* general client client
		 * must be in active_clients list.
		 */
		client = virtio_heci_find_client(vheci, heci_hdr->host_addr);
		if (!client) {
			DPRINTF(("vheci: TX: NO client mapping "
				"for ME[%d]!\n\r", heci_hdr->host_addr));
			goto failed;
		}

		DPRINTF(("vheci: TX: found ME[%d]fd[%d]:FE[%d]\n\r",
				client->client_id, client->client_fd,
				client->client_addr));
		if (client->send_buf_sz - client->send_idx < heci_hdr->length) {
			DPRINTF(("vheci: TX: ME[%d]fd[%d] overflow "
					"max_message_length in sendbuf!\n\r",
					client->client_id, client->client_fd));
			/* close the connection according to spec */
			virtio_heci_disconnect_vclient(vheci, client);
			goto out;
		}
		/* copy buffer from virtqueue to send_buf */
		memcpy(client->send_buf + client->send_idx,
			(uint8_t *)iov[1].iov_base, heci_hdr->length);
		client->send_idx += heci_hdr->length;
		if (heci_hdr->msg_complete) {
			/* send complete msg to HW */
			DPRINTF(("vheci: TX: ME[%d]fd[%d] complete msg,"
					" pass send_buf to MEI!\n\r",
					client->client_id, client->client_fd));
			len = native_heci_write(client);
			if (len != client->send_idx) {
				DPRINTF(("vheci: TX: ME[%d]fd[%d] "
						" write data to MEI fail!\n\r",
					client->client_id, client->client_fd));
				client->send_idx -= heci_hdr->length;
				goto send_failed;
			}
			/* reset index to indicate send buffer is empty */
			client->send_idx = 0;

			/* Give guest a flow control credit */
			flow_ctl_req.hbm_cmd.cmd = HECI_HBM_FLOW_CONTROL;
			flow_ctl_req.me_addr = client->client_id;
			/*
			 * single recv buffer client,
			 */
			if (client->props.single_recv_buf)
				flow_ctl_req.host_addr = 0;
			else
				flow_ctl_req.host_addr = client->client_addr;
			hbm_client = virtio_heci_get_hbm_client(vheci);
			if (hbm_client) {
				populate_heci_hdr(hbm_client, &hdr,
					sizeof(struct heci_hbm_flow_ctl), 1);
				DPRINTF(("vheci: TX: DM(flow ctl,ME[%d]fd[%d]) "
					 "-> UOS(FE[%d]): len[%d] last[%d]\n\r",
					client->client_id,
					client->client_fd, client->client_addr,
					hdr.length, hdr.msg_complete));
				print_hex((uint8_t *)&flow_ctl_req,
						sizeof(flow_ctl_req));
				virtio_heci_hbm_response(vheci, &hdr,
					&flow_ctl_req, sizeof(flow_ctl_req));
				virtio_heci_client_put(vheci, hbm_client);
			}
		}
out:
		virtio_heci_client_put(vheci, client);
	}

	/* chain is processed, release it and set tlen */
	vq_relchain(vq, idx, tlen);
	DPRINTF(("vheci: TX: release OUT-vq idx[%d]\r\n", idx));

	if (vheci->rx_need_sched) {
		pthread_mutex_lock(&vheci->rx_mutex);
		pthread_cond_signal(&vheci->rx_cond);
		pthread_mutex_unlock(&vheci->rx_mutex);
	}

	return;
send_failed:
	virtio_heci_client_put(vheci, client);
failed:
	if (vheci->status == VHECI_PENDING_RESET) {
		virtio_heci_virtual_fw_reset(vheci);
		/* Let's wait 100ms for HBM enumeration done */
		usleep(100000);
		virtio_config_changed(&vheci->base);
	}
	/* drop the data */
	vq_relchain(vq, idx, tlen);
}


/*
 * Thread which will handle processing of TX desc
 */
static void *virtio_heci_tx_thread(void *param)
{
	struct virtio_heci *vheci = param;
	struct virtio_vq_info *vq;
	int error;

	vq = &vheci->vqs[VIRTIO_HECI_TXQ];

	/*
	 * Let us wait till the tx queue pointers get initialized &
	 * first tx signaled
	 */
	pthread_mutex_lock(&vheci->tx_mutex);
	error = pthread_cond_wait(&vheci->tx_cond, &vheci->tx_mutex);
	assert(error == 0);

	while (vheci->status != VHECI_DEINIT) {
		/* note - tx mutex is locked here */
		while (!vq_has_descs(vq)) {
			vq->used->flags &= ~VRING_USED_F_NO_NOTIFY;
			mb();
			if (vq_has_descs(vq) &&
				vheci->status != VHECI_RESET)
				break;

			error = pthread_cond_wait(&vheci->tx_cond,
					&vheci->tx_mutex);
			assert(error == 0);
			if (vheci->status == VHECI_DEINIT)
				goto out;
		}
		vq->used->flags |= VRING_USED_F_NO_NOTIFY;
		pthread_mutex_unlock(&vheci->tx_mutex);

		do {
			/*
			 * Run through entries, send them
			 */
			virtio_heci_proc_tx(vheci, vq);
		} while (vq_has_descs(vq));

		vq_endchains(vq, 1);

		pthread_mutex_lock(&vheci->tx_mutex);
	}
out:
	pthread_mutex_unlock(&vheci->tx_mutex);
	pthread_exit(NULL);
}

/*
 * Process the data received from native mei cdev and hbm emulation
 * handler, assemable related heci header then copy to rx virtqueue.
 */
static void
virtio_heci_proc_vclient_rx(struct virtio_heci_client *client,
		struct virtio_vq_info *vq)
{
	struct iovec iov[VIRTIO_HECI_RXSEGS + 1];
	struct heci_msg_hdr *heci_hdr;
	uint16_t idx = 0;
	int n, len = 0;
	uint8_t *buf;

	n = vq_getchain(vq, &idx, iov, VIRTIO_HECI_RXSEGS, NULL);
	assert(n == VIRTIO_HECI_RXSEGS);

	if (client->type == TYPE_HBM) {
		/* HBM client has data to FE */
		len = client->recv_offset - client->recv_handled;
		memcpy(iov[0].iov_base,
				client->recv_buf + client->recv_handled, len);
		client->recv_offset = client->recv_handled = 0;
		DPRINTF(("vheci: RX: DM:ME[%d]fd[%d]off[%d]"
					"-> UOS:client_addr[%d] len[%d]\n\r",
					client->client_id, client->client_fd,
					client->recv_handled,
					client->client_addr, len));
		goto out;
	}

	/* NORMAL client buffer length need remove HECI header */
	len = (VIRTIO_HECI_FIFOSZ - 1) * sizeof(struct heci_msg_hdr);
	buf = (uint8_t *)iov[0].iov_base + sizeof(struct heci_msg_hdr);
	heci_hdr = (struct heci_msg_hdr *)iov[0].iov_base;

	if (client->recv_offset - client->recv_handled > len) {
		populate_heci_hdr(client, heci_hdr, len, 0);
		memcpy(buf, client->recv_buf + client->recv_handled, len);
		client->recv_handled += len;
		len += sizeof(struct heci_msg_hdr);
		DPRINTF(("vheci: RX: data(partly) DM:ME[%d]fd[%d]off[%d]"
					" -> UOS:client_addr[%d] len[%d]\n\r",
					client->client_id, client->client_fd,
					client->recv_handled,
					client->client_addr, len));
	} else {
		/* this client has data to guest, can be in one recv buf */
		len = client->recv_offset - client->recv_handled;
		populate_heci_hdr(client, heci_hdr, len, 1);
		memcpy(buf, client->recv_buf + client->recv_handled, len);
		client->recv_offset = client->recv_handled = 0;
		client->recv_creds--;
		len += sizeof(struct heci_msg_hdr);
		DPRINTF(("vheci: RX: data(end) DM:ME[%d]fd[%d]off[%d]"
					" -> UOS:client_addr[%d] len[%d]\n\r",
					client->client_id, client->client_fd,
					client->recv_handled,
					client->client_addr, len));
	}
out:
	vq_relchain(vq, idx, len);
}

/* caller need hold rx_mutex
 * return:
 *	true  - data processed
 *	fasle - need resched
 */
static bool
virtio_heci_proc_rx(struct virtio_heci *vheci,
		struct virtio_vq_info *vq)
{
	struct virtio_heci_client *pclient, *client = NULL;

	/*
	 * Traverse the list find all available clients, ignore these normal
	 * type clients, which have data but it's receieve creds is Zero.
	 */
	pthread_mutex_lock(&vheci->list_mutex);
	LIST_FOREACH(pclient, &vheci->active_clients, list) {
		if ((pclient->recv_offset - pclient->recv_handled > 0) &&
		    (pclient->recv_creds > 0 || pclient->type == TYPE_HBM)) {
			client = virtio_heci_client_get(pclient);
			break;
		}
	}
	pthread_mutex_unlock(&vheci->list_mutex);

	/* no client has data need to be processed */
	if (!client) {
		DPRINTF(("vheci: RX: no available client!\n\r"));
		return true;
	}

	virtio_heci_proc_vclient_rx(client, vq);
	virtio_heci_client_put(vheci, client);
	return false;
}

/*
 * Thread which will handle processing of RX desc
 */
static void *virtio_heci_rx_thread(void *param)
{
	struct virtio_heci *vheci = param;
	struct virtio_vq_info *vq;
	int error;

	vq = &vheci->vqs[VIRTIO_HECI_RXQ];

	/*
	 * Let us wait till the rx queue pointers get initialised &
	 * first tx signaled
	 */
	pthread_mutex_lock(&vheci->rx_mutex);
	error = pthread_cond_wait(&vheci->rx_cond, &vheci->rx_mutex);
	assert(error == 0);

	while (vheci->status != VHECI_DEINIT) {
		/* note - rx mutex is locked here */
		while (vq_ring_ready(vq)) {
			vq->used->flags &= ~VRING_USED_F_NO_NOTIFY;
			mb();
			if (vq_has_descs(vq) &&
				vheci->rx_need_sched &&
				vheci->status != VHECI_RESET)
				break;

			error = pthread_cond_wait(&vheci->rx_cond,
					&vheci->rx_mutex);
			assert(error == 0);
			if (vheci->status == VHECI_DEINIT)
				goto out;
		}
		vq->used->flags |= VRING_USED_F_NO_NOTIFY;

		do {
			if (virtio_heci_proc_rx(vheci, vq))
				vheci->rx_need_sched = false;
		} while (vq_has_descs(vq) && vheci->rx_need_sched);

		if (!vq_has_descs(vq))
			vq_endchains(vq, 1);

	}
out:
	pthread_mutex_unlock(&vheci->rx_mutex);
	pthread_exit(NULL);
}
static void
virtio_heci_notify_rx(void *heci, struct virtio_vq_info *vq)
{
	struct virtio_heci *vheci = heci;
	/*
	 * Any ring entries to process?
	 */
	if (!vq_has_descs(vq))
		return;

	/* Signal the rx thread for processing */
	pthread_mutex_lock(&vheci->rx_mutex);
	DPRINTF(("vheci: RX: New IN buffer available!\n\r"));
	vq->used->flags |= VRING_USED_F_NO_NOTIFY;
	pthread_cond_signal(&vheci->rx_cond);
	pthread_mutex_unlock(&vheci->rx_mutex);
}

static void
virtio_heci_notify_tx(void *heci, struct virtio_vq_info *vq)
{
	struct virtio_heci *vheci = heci;
	/*
	 * Any ring entries to process?
	 */
	if (!vq_has_descs(vq))
		return;

	/* Signal the tx thread for processing */
	pthread_mutex_lock(&vheci->tx_mutex);
	DPRINTF(("vheci: TX: New OUT buffer available!\n\r"));
	vq->used->flags |= VRING_USED_F_NO_NOTIFY;
	pthread_cond_signal(&vheci->tx_cond);
	pthread_mutex_unlock(&vheci->tx_mutex);
}

static int
virtio_heci_start(struct virtio_heci *vheci)
{
	int status = 0;

	vheci->hbm_client = virtio_heci_create_client(vheci, 0, 0, &status);
	if (!vheci->hbm_client)
		return -1;
	vheci->config->hw_ready = 1;
	virtio_heci_set_status(vheci, VHECI_READY);

	return 0;
}

static int
virtio_heci_stop(struct virtio_heci *vheci)
{
	virtio_heci_set_status(vheci, VHECI_DEINIT);
	pthread_mutex_lock(&vheci->tx_mutex);
	pthread_cond_signal(&vheci->tx_cond);
	pthread_mutex_unlock(&vheci->tx_mutex);

	pthread_mutex_lock(&vheci->rx_mutex);
	pthread_cond_signal(&vheci->rx_cond);
	pthread_mutex_unlock(&vheci->rx_mutex);

	virtio_heci_virtual_fw_reset(vheci);

	pthread_join(vheci->rx_thread, NULL);
	pthread_join(vheci->tx_thread, NULL);

	pthread_mutex_destroy(&vheci->rx_mutex);
	pthread_mutex_destroy(&vheci->tx_mutex);
	pthread_mutex_destroy(&vheci->list_mutex);
	return 0;
}

static void
virtio_heci_virtual_fw_reset(struct virtio_heci *vheci)
{
	struct virtio_heci_client *client = NULL, *pclient = NULL;

	virtio_heci_set_status(vheci, VHECI_RESET);
	vheci->config->hw_ready = 0;

	pthread_mutex_lock(&vheci->list_mutex);
	list_foreach_safe(client, &vheci->active_clients, list, pclient)
		virtio_heci_client_put(vheci, client);
	pthread_mutex_unlock(&vheci->list_mutex);

	/* clients might be referred by other threads here
	 * Let's wait all clients destroyed
	 */
	while (!LIST_EMPTY(&vheci->active_clients))
		;
	vheci->hbm_client = NULL;
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
	struct virtio_heci *vheci = vsc;

	if (offset == offsetof(struct virtio_heci_config, host_reset)) {
		if (size == sizeof(uint8_t) && val == 1) {
			DPRINTF(("vheci cfgwrite: host_reset [%d]\r\n", val));
			/* guest initate reset need restart */
			virtio_heci_virtual_fw_reset(vheci);
			virtio_heci_start(vheci);
			virtio_config_changed(&vheci->base);
		}
	}
	return 0;
}

static int
virtio_heci_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_heci *vheci;
	char tname[MAXCOMLEN + 1];
	pthread_mutexattr_t attr;
	int i, rc;

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
	if (virtio_uses_msix()) {
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

	if (virtio_interrupt_init(&vheci->base, virtio_uses_msix()))
		goto setup_fail;
	virtio_set_io_bar(&vheci->base, 0);

	/*
	 * tx stuff, thread, mutex, cond
	 */
	pthread_mutex_init(&vheci->tx_mutex, &attr);
	pthread_cond_init(&vheci->tx_cond, NULL);
	pthread_create(&vheci->tx_thread, NULL,
		virtio_heci_tx_thread, (void *)vheci);
	snprintf(tname, sizeof(tname), "vheci-%d:%d tx", dev->slot, dev->func);
	pthread_setname_np(vheci->tx_thread, tname);

	/*
	 * rx stuff
	 */
	pthread_mutex_init(&vheci->rx_mutex, &attr);
	pthread_cond_init(&vheci->rx_cond, NULL);
	pthread_create(&vheci->rx_thread, NULL,
			virtio_heci_rx_thread, (void *)vheci);
	snprintf(tname, sizeof(tname), "vheci-%d:%d rx", dev->slot, dev->func);
	pthread_setname_np(vheci->rx_thread, tname);

	/*
	 * init clients
	 */
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&vheci->list_mutex, &attr);
	LIST_INIT(&vheci->active_clients);

	/*
	 * start heci backend
	 */
	if (virtio_heci_start(vheci) < 0)
		goto start_fail;

	return 0;
start_fail:
	virtio_heci_stop(vheci);
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

	virtio_heci_stop(vheci);
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
