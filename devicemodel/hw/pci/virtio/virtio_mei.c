/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
/*
 * MEI device virtualization.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>

#include <linux/uuid.h>
#include <linux/mei.h>

#include "types.h"
#include "vmmapi.h"
#include "mevent.h"
#include "pci_core.h"
#include "virtio.h"
#include "dm.h"

#include "mei.h"

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#define DEV_NAME_SIZE sizeof(((struct dirent *)0)->d_name)

#ifndef UUID_STR_LEN
#define UUID_STR_LEN 37
#endif

#ifndef GUID_INIT
#define GUID_INIT(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
	UUID_LE(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7)
#endif

static int guid_parse(const char *str, size_t maxlen, guid_t *guid)
{
	const char *p = "00000000-0000-0000-0000-000000000000";
	const size_t len = strnlen(p, UUID_STR_LEN);
	uint32_t a;
	uint16_t b, c;
	uint8_t d[2], e[6];
	char buf[3];
	unsigned int i;

	if (strnlen(str, maxlen) != len)
		return -1;

	for (i = 0; i < len; i++) {
		if (str[i] == '-') {
			if (p[i] == '-')
				continue;
			else
				return -1;
		} else if (!isxdigit(str[i])) {
			return -1;
		}
	}

	a = strtoul(str +  0, NULL, 16);
	b = strtoul(str +  9, NULL, 16);
	c = strtoul(str + 14, NULL, 16);

	buf[2] = 0;
	for (i = 0; i < 2; i++) {
		buf[0] = str[19 + i * 2];
		buf[1] = str[19 + i * 2 + 1];
		d[i] = strtoul(buf, NULL, 16);
	}

	for (i = 0; i < 6; i++) {
		buf[0] = str[24 + i * 2];
		buf[1] = str[24 + i * 2 + 1];
		e[i] = strtoul(buf, NULL, 16);
	}

	*guid = GUID_INIT(a, b, c,
			  d[0], d[1], e[0], e[1], e[2], e[3], e[4], e[5]);

	return 0;
}

static int guid_unparse(const guid_t *guid, char *str, size_t len)
{
	unsigned int i;
	size_t pos = 0;

	if (len < UUID_STR_LEN)
		return -EINVAL;

	pos += snprintf(str + pos, len - pos, "%02x", guid->b[3]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[2]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[1]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[0]);
	str[pos] = '-';
	pos++;
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[5]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[4]);
	str[pos] = '-';
	pos++;
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[7]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[6]);
	str[pos] = '-';
	pos++;
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[8]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[9]);
	str[pos] = '-';
	pos++;
	for (i = 10; i < 16; i++)
		pos += snprintf(str + pos, len - pos, "%02x", guid->b[i]);

	return 0;
}

struct refcnt {
	void (*destroy)(const struct refcnt *ref);
	int count;
};

static inline void
refcnt_get(const struct refcnt *ref)
{
	__sync_add_and_fetch((int *)&ref->count, 1);
}

static inline void
refcnt_put(const struct refcnt *ref)
{
	if (__sync_sub_and_fetch((int *)&ref->count, 1) == 0)
		ref->destroy(ref);
}

static int vmei_debug;
static FILE *vmei_dbg_file;

#define DPRINTF(format, arg...)  do {                           \
	if (vmei_debug && vmei_dbg_file) {                      \
		fprintf(vmei_dbg_file, "vmei: %s: " format,     \
			__func__, ##arg);                       \
		fflush(vmei_dbg_file);                          \
	}                                                       \
} while (0)

#define WPRINTF(format, arg...) do {                            \
	fprintf(stderr, "vmei: %s: " format,  __func__, ##arg); \
	if (vmei_dbg_file) {                                    \
		fprintf(vmei_dbg_file, "vmei: %s: " format,     \
			__func__, ##arg);                       \
		fflush(vmei_dbg_file);                          \
	}                                                       \
} while (0)

#define MEI_HCL_FMT(format) "CL:[%02d:%02d](%04d): " format
#define MEI_HCL_PRM(_hcl) (_hcl)->host_addr, (_hcl)->me_addr, (_hcl)->client_fd
#define HCL_DBG(_hcl, format, arg...) \
	DPRINTF(MEI_HCL_FMT(format), MEI_HCL_PRM(_hcl), ##arg)
#define HCL_WARN(_hcl, format, arg...) \
	WPRINTF(MEI_HCL_FMT(format), MEI_HCL_PRM(_hcl), ##arg)

static void vmei_dbg_print_hex(const char *title,
			       const void *data, size_t length)
{
	const unsigned char *bytes = data;
	FILE *dbg_file;
	size_t i;

	if (vmei_debug < 2)
		return;

	dbg_file = (vmei_dbg_file) ? vmei_dbg_file : stdout;

	if (title)
		fprintf(dbg_file, "%s ", title);

	for (i = 0; i < length; i++) {
		if (i % 16 == 0 && i != 0)
			fprintf(dbg_file, "\n");
		fprintf(dbg_file, "%02x ", bytes[i]);
	}
	fprintf(dbg_file, "\n");
}

static void
vmei_dbg_client_properties(const struct mei_client_properties *prop)
{
	char guid_str[UUID_STR_LEN] = {0};

	if (vmei_debug < 2)
		return;

	guid_unparse(&prop->protocol_name, guid_str, UUID_STR_LEN);

	DPRINTF("client properties:\n");
	DPRINTF("\t fixed_address:      %d\n", prop->fixed_address);
	DPRINTF("\t protocol_version:   %d\n", prop->protocol_version);
	DPRINTF("\t max_connections:    %d\n", prop->max_connections);
	DPRINTF("\t single_recv_buf:    %d\n", prop->single_recv_buf);
	DPRINTF("\t max_msg_length:     %u\n", prop->max_msg_length);
	DPRINTF("\t protocol_name:      %s\n", guid_str);
}

#define MEI_FW_STATUS_MAX 6

#define VMEI_VQ_NUM   2
#define VMEI_RXQ      0
#define VMEI_TXQ      1
#define VMEI_RX_SEGS  1
#define VMEI_TX_SEGS  2

/*
 * MEI HW max support FIFO depth is 128
 * We might support larger depth, which need change MEI driver
 */

#define VMEI_BUF_DEPTH  128
#define VMEI_SLOT_SZ      4
#define VMEI_BUF_SZ      (VMEI_BUF_DEPTH * VMEI_SLOT_SZ)
#define VMEI_RING_SZ     64

/**
 * VMEI supported HBM version
 */
#define VMEI_HBM_VER_MAJ 2
#define VMEI_HBM_VER_MIN 0

#define MEI_SYSFS_ROOT "/sys/class/mei"
#define MEI_DEV_STATE_LEN 12

struct mei_virtio_cfg {
	uint32_t buf_depth;
	uint8_t hw_ready;
	uint8_t host_reset;
	uint8_t reserved[2];
	uint32_t fw_status[MEI_FW_STATUS_MAX];
} __attribute__((packed));

#define VMEI_IOBUFS_MAX 8
struct vmei_circular_iobufs {
	struct iovec                       bufs[VMEI_IOBUFS_MAX];
	uint8_t                            complete[VMEI_IOBUFS_MAX];
	size_t                             buf_sz;
	uint8_t                            i_idx; /* insert index */
	uint8_t                            r_idx; /* remove index */
};

struct virtio_mei;

struct vmei_host_client {
	struct refcnt                ref;
	struct vmei_me_client        *mclient;
	LIST_ENTRY(vmei_host_client) list;

	uint8_t                      me_addr;
	uint8_t                      host_addr;
	uint8_t                      reserved[2];
	int                          client_fd;

	struct mevent                *rx_mevp;
	uint8_t                      *recv_buf;
	size_t                       recv_buf_sz;
	int                          recv_offset;
	int                          recv_handled;
	int                          recv_creds;

	struct vmei_circular_iobufs  send_bufs;
};

struct vmei_me_client {
	struct refcnt                ref;
	struct virtio_mei            *vmei;
	LIST_ENTRY(vmei_me_client)   list;

	uint8_t                      client_id;
	struct mei_client_properties props;
	pthread_mutex_t              list_mutex;
	LIST_HEAD(connhead, vmei_host_client) connections;
};

enum vmei_status {
	VMEI_STS_READY,
	VMEI_STS_PENDING_RESET,
	VMEI_STS_RESET,
	VMEI_STST_DEINIT
};

struct virtio_mei {
	struct virtio_base              base;
	char                            name[16];
	char                            devnode[32];
	struct mei_enumerate_me_clients me_clients_map;
	struct virtio_vq_info           vqs[VMEI_VQ_NUM];
	volatile enum vmei_status       status;
	uint8_t                         vtag;

	pthread_mutex_t mutex;

	struct mevent                   *reset_mevp;

	struct mei_virtio_cfg           *config;

	pthread_t                       tx_thread;
	pthread_mutex_t                 tx_mutex;
	pthread_cond_t                  tx_cond;

	pthread_t                       rx_thread;
	pthread_mutex_t                 rx_mutex;
	pthread_cond_t                  rx_cond;
	bool                            rx_need_sched;

	pthread_mutex_t                 list_mutex;
	LIST_HEAD(clhead, vmei_me_client) active_clients;
	struct vmei_me_client           *hbm_client;
	int hbm_fd;
};

static inline void
vmei_set_status(struct virtio_mei *vmei, enum vmei_status status)
{
	if (status == VMEI_STST_DEINIT ||
	    status == VMEI_STS_READY ||
	    status > vmei->status)
		vmei->status = status;
}

static void
vmei_host_client_destroy(const struct refcnt *ref)
{
	struct vmei_host_client *hclient;
	struct vmei_me_client *mclient;
	unsigned int i;

	hclient = container_of(ref, struct vmei_host_client, ref);

	mclient = hclient->mclient;

	pthread_mutex_lock(&mclient->list_mutex);
	LIST_REMOVE(hclient, list);
	pthread_mutex_unlock(&mclient->list_mutex);

	if (hclient->rx_mevp)
		mevent_delete(hclient->rx_mevp);
	if (hclient->client_fd > -1)
		close(hclient->client_fd);
	for (i = 0; i < VMEI_IOBUFS_MAX; i++)
		free(hclient->send_bufs.bufs[i].iov_base);
	free(hclient->recv_buf);
	free(hclient);
}

static struct vmei_host_client *
vmei_host_client_create(struct vmei_me_client *mclient, uint8_t host_addr)
{
	struct vmei_host_client *hclient;
	size_t size = mclient->props.max_msg_length;
	unsigned int i;

	hclient = calloc(1, sizeof(*hclient));
	if (!hclient)
		return NULL;

	hclient->ref = (struct refcnt){vmei_host_client_destroy, 1};

	hclient->me_addr   = mclient->client_id;
	hclient->host_addr = host_addr;
	hclient->mclient   = mclient;
	hclient->client_fd = -1;
	hclient->rx_mevp   = NULL;

	/* HBM and fixed address doesn't provide flow control
	 * make the receiving part always available.
	 */
	if (host_addr == 0)
		hclient->recv_creds = 1;

	/* setup send_buf and recv_buf for the client */
	for (i = 0; i < VMEI_IOBUFS_MAX; i++) {
		hclient->send_bufs.bufs[i].iov_base = calloc(1, size);
		if (!hclient->send_bufs.bufs[i].iov_base) {
			HCL_WARN(hclient, "allocation failed\n");
			goto fail;
		}
	}

	hclient->recv_buf = calloc(1, size);
	if (!hclient->recv_buf) {
		HCL_WARN(hclient, "allocation failed\n");
		goto fail;
	}
	hclient->send_bufs.buf_sz = size;
	hclient->recv_buf_sz = size;

	pthread_mutex_lock(&mclient->list_mutex);
	LIST_INSERT_HEAD(&mclient->connections, hclient, list);
	pthread_mutex_unlock(&mclient->list_mutex);

	return hclient;

fail:
	for (i = 0; i < VMEI_IOBUFS_MAX; i++)
		free(hclient->send_bufs.bufs[i].iov_base);
	memset(&hclient->send_bufs, 0, sizeof(hclient->send_bufs));
	free(hclient->recv_buf);
	free(hclient);

	return NULL;
}

static struct vmei_host_client*
vmei_host_client_get(struct vmei_host_client *hclient)
{
	refcnt_get(&hclient->ref);
	return hclient;
}

static void
vmei_host_client_put(struct vmei_host_client *hclient)
{
	refcnt_put(&hclient->ref);
}

struct virtio_mei *
vmei_host_client_to_vmei(struct vmei_host_client *hclient)
{
	if (hclient && hclient->mclient)
		return hclient->mclient->vmei;

	return NULL;
}

static void
vmei_del_me_client(struct vmei_me_client *mclient)
{
	struct virtio_mei *vmei = mclient->vmei;

	pthread_mutex_lock(&vmei->list_mutex);
	LIST_REMOVE(mclient, list);
	pthread_mutex_unlock(&vmei->list_mutex);
}

static void
vmei_me_client_destroy_host_clients(struct vmei_me_client *mclient)
{
	struct vmei_host_client *e, *n;

	pthread_mutex_lock(&mclient->list_mutex);
	e = LIST_FIRST(&mclient->connections);
	while (e) {
		n = LIST_NEXT(e, list);
		vmei_host_client_put(e);
		e = n;
	}
	pthread_mutex_unlock(&mclient->list_mutex);
	LIST_INIT(&mclient->connections);
}

static void
vmei_me_client_destroy(const struct refcnt *ref)
{
	struct vmei_me_client *mclient;

	mclient = container_of(ref, struct vmei_me_client, ref);

	vmei_del_me_client(mclient);
	vmei_me_client_destroy_host_clients(mclient);
	pthread_mutex_destroy(&mclient->list_mutex);
	free(mclient);
}

static struct vmei_me_client *
vmei_me_client_create(struct virtio_mei *vmei, uint8_t client_id,
		      const struct mei_client_properties *props)
{
	struct vmei_me_client *mclient;
	pthread_mutexattr_t attr;

	if (!props)
		return NULL;

	mclient = calloc(1, sizeof(*mclient));
	if (!mclient)
		return mclient;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mclient->list_mutex, &attr);
	pthread_mutexattr_destroy(&attr);

	mclient->vmei = vmei;
	mclient->client_id = client_id;
	mclient->ref = (struct refcnt){vmei_me_client_destroy, 1};

	memcpy(&mclient->props, props, sizeof(*props));
	vmei_dbg_client_properties(&mclient->props);

	DPRINTF("ME client created %d\n", client_id);

	return mclient;
}

static struct vmei_me_client*
vmei_me_client_get(struct vmei_me_client *mclient)
{
	refcnt_get(&mclient->ref);
	return mclient;
}

static void
vmei_me_client_put(struct vmei_me_client *mclient)
{
	refcnt_put(&mclient->ref);
}

static struct vmei_host_client*
vmei_me_client_get_host_client(struct vmei_me_client *mclient,
			       uint8_t host_addr)
{
	struct vmei_host_client *e, *hclient = NULL;

	pthread_mutex_lock(&mclient->list_mutex);
	LIST_FOREACH(e, &mclient->connections, list) {
		if (e->host_addr == host_addr) {
			hclient = vmei_host_client_get(e);
			break;
		}
	}
	pthread_mutex_unlock(&mclient->list_mutex);

	return hclient;
}

static void vmei_add_me_client(struct vmei_me_client *mclient)
{
	struct virtio_mei *vmei = mclient->vmei;

	pthread_mutex_lock(&vmei->list_mutex);
	if (mclient->client_id == 0)
		/* make sure hbm client at the head of the list */
		LIST_INSERT_HEAD(&vmei->active_clients, mclient, list);
	else
		LIST_INSERT_AFTER(LIST_FIRST(&vmei->active_clients),
				  mclient, list);
	pthread_mutex_unlock(&vmei->list_mutex);
}

static struct vmei_me_client *
vmei_find_me_client(struct virtio_mei *vmei, uint8_t client_id)
{
	struct vmei_me_client *e, *mclient = NULL;

	pthread_mutex_lock(&vmei->list_mutex);
	LIST_FOREACH(e, &vmei->active_clients, list) {
		if (e->client_id == client_id) {
			mclient = vmei_me_client_get(e);
			break;
		}
	}
	pthread_mutex_unlock(&vmei->list_mutex);
	return mclient;
}


static struct vmei_host_client*
vmei_find_host_client(struct virtio_mei *vmei,
		      uint8_t me_addr, uint8_t host_addr)
{
	struct vmei_me_client *mclient;
	struct vmei_host_client *hclient;

	mclient = vmei_find_me_client(vmei, me_addr);
	if (!mclient) {
		DPRINTF("client with me address %d was not found\n", me_addr);
		return NULL;
	}

	hclient = vmei_me_client_get_host_client(mclient, host_addr);
	if (!hclient)
		DPRINTF("host client %d does not exists!\n", host_addr);

	vmei_me_client_put(mclient);

	return hclient;
}

static void vmei_free_me_clients(struct virtio_mei *vmei)
{
	struct vmei_me_client *e, *n;

	pthread_mutex_lock(&vmei->list_mutex);
	e = LIST_FIRST(&vmei->active_clients);
	while (e) {
		n = LIST_NEXT(e, list);
		vmei_me_client_put(e);
		e = n;
	}
	LIST_INIT(&vmei->active_clients);
	pthread_mutex_unlock(&vmei->list_mutex);
}

/**
 *  vmei_get_free_me_id() - search for free me id in clients map
 *
 * @vmei: virtio mei device
 * @fixed: is a a fixed address client
 *
 * Return: free me id, 0 - if none found
 */
static uint8_t
vmei_clients_map_find_free(struct virtio_mei *vmei, bool fixed)
{
	unsigned int octet, bit;
	uint8_t *valid_addresses = vmei->me_clients_map.valid_addresses;

	for (octet = fixed ? 0 : 4; octet < 32; octet++) {
		for (bit = 0; bit < 8; bit++) {
			/* ID 0 is reserved and should not be allocated*/
			if (!octet && !bit)
				continue;
			if (!(valid_addresses[octet] & BIT(bit)))
				return octet * 8 + bit;
		}
	}
	return 0;
}

/**
 * vmei_clients_map_update() - set or unset me id in the valid address map
 *
 * @vmei: virtio mei device
 * @me_id: me client id
 * @set: set or unset the client id
 */
static void
vmei_clients_map_update(struct virtio_mei *vmei, uint8_t me_id, bool set)
{
	unsigned int octet, bit;
	uint8_t *valid_addresses = vmei->me_clients_map.valid_addresses;

	octet = me_id / 8;
	bit = me_id % 8;

	if (set)
		valid_addresses[octet] |= BIT(bit);
	else
		valid_addresses[octet] &= ~BIT(bit);
}

static int mei_sysfs_read_property_file(const char *fname, char *buf, size_t sz)
{
	int fd;
	int rc;

	if (!buf)
		return -EINVAL;

	if (!sz)
		return 0;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		DPRINTF("open failed %s %d\n", fname, errno);
		return -1;
	}

	rc = read(fd, buf, sz);

	close(fd);

	return rc;
}

static int mei_sysfs_read_property_u8(const char *fname, uint8_t *u8_property)
{
	char buf[4] = {0};
	unsigned long int res;

	if (mei_sysfs_read_property_file(fname, buf, sizeof(buf) - 1) < 0)
		return -1;

	res = strtoul(buf, NULL, 10);
	if (res >= 256)
		return -1;

	*u8_property = (uint8_t)res;

	return 0;
}

static int mei_sysfs_read_property_u32(const char *fname,
				       uint32_t *u32_property)
{
	char buf[32] = {0};
	unsigned long int res;

	if (mei_sysfs_read_property_file(fname, buf, sizeof(buf) - 1) < 0)
		return -1;

	res = strtoul(buf, NULL, 10);
	if (res == ULONG_MAX)
		return -1;

	*u32_property = res;

	return 0;
}

static int mei_sysfs_read_property_uuid(char *fname, guid_t *uuid)
{
	char buf[UUID_STR_LEN] = {0};

	if (mei_sysfs_read_property_file(fname, buf, sizeof(buf) - 1) < 0)
		return -1;

	return guid_parse(buf, sizeof(buf), uuid);
}

static int mei_sysfs_read_properties(char *devpath, size_t size, size_t offset,
				     struct mei_client_properties *props)
{
	uint32_t max_msg_length;

	snprintf(&devpath[offset], size - offset, "%s", "uuid");
	if (mei_sysfs_read_property_uuid(devpath, &props->protocol_name) < 0)
		return -EFAULT;

	snprintf(&devpath[offset], size - offset, "%s", "version");
	if (mei_sysfs_read_property_u8(devpath, &props->protocol_version) < 0)
		return -EFAULT;

	snprintf(&devpath[offset], size - offset, "%s", "fixed");
	if (mei_sysfs_read_property_u8(devpath, &props->fixed_address) < 0)
		props->fixed_address = 0;

	/* Use local variable as function is unaware of the unaligned pointer */
	snprintf(&devpath[offset], size - offset, "%s", "max_len");
	if (mei_sysfs_read_property_u32(devpath, &max_msg_length) < 0)
		max_msg_length = 512;
	props->max_msg_length = max_msg_length;

	snprintf(&devpath[offset], size - offset, "%s", "max_conn");
	if (mei_sysfs_read_property_u8(devpath, &props->max_connections) < 0)
		props->max_connections = 1;

	return 0;
}
static bool is_prefix(const char *prfx, const char *str, size_t maxlen)
{
	if (!prfx || !str || prfx[0] == '\0')
		return false;

	return strncmp(prfx, str, maxlen);
}

static int vmei_me_client_scan_list(struct virtio_mei *vmei)
{
	DIR *dev_dir = NULL;
	struct dirent *ent;
	char devpath[256];
	int d_offset, c_offset;
	uint8_t me_id = 1;
	struct vmei_me_client *mclient;
	uint8_t vtag, vtag_supported = 0;

	d_offset = snprintf(devpath, sizeof(devpath) - 1, "%s/%s/%s",
			    MEI_SYSFS_ROOT, vmei->name, "device/");
	if (d_offset < 0)
		return -1;

	dev_dir = opendir(devpath);
	if (!dev_dir) {
		WPRINTF("opendir failed %d", errno);
		return -1;
	}
	/*
	 * iterate over device directory and find the directories
	 * starting with "mei::" - those are the clients.
	 */
	while ((ent = readdir(dev_dir)) != NULL) {

		if (ent->d_type == DT_DIR &&
		    is_prefix("mei::", ent->d_name, DEV_NAME_SIZE)) {

			struct mei_client_properties props;

			memset(&props, 0, sizeof(props));

			DPRINTF("found client %s %s\n", ent->d_name, devpath);

			c_offset = snprintf(&devpath[d_offset],
					    sizeof(devpath) - d_offset,
					    "%s/", ent->d_name);
			if (c_offset < 0)
				continue;
			c_offset += d_offset;

			vtag = 0;
			snprintf(&devpath[c_offset],
				 sizeof(devpath) - c_offset, "%s", "vtag");
			if (mei_sysfs_read_property_u8(devpath, &vtag) < 0)
				vtag = 0;

			if (!vtag)
				continue;

			if (mei_sysfs_read_properties(devpath,
						      sizeof(devpath), c_offset,
						      &props) < 0)
				continue;

			me_id = vmei_clients_map_find_free(vmei,
							   props.fixed_address);

			mclient = vmei_me_client_create(vmei, me_id, &props);
			if (!mclient)
				continue;

			vmei_clients_map_update(vmei, me_id, true);
			vmei_add_me_client(mclient);

			vtag_supported = 1;
		}
	}

	vmei_dbg_print_hex("me_clients_map",
			   vmei->me_clients_map.valid_addresses,
			    sizeof(vmei->me_clients_map.valid_addresses));

	closedir(dev_dir);

	/*
	 * Don't return error in order not to the crash DM;
	 * currently it cannot deal with single driver error.
	 */
	if (!vtag_supported)
		WPRINTF("The platform doesn't support vtags!!!\n");

	return 0;
}

static int
vmei_read_fw_status(struct virtio_mei *vmei, uint32_t *fw_status)
{
	int offset, i;
	unsigned long int res;
	char path[256];
	/* each value consist of 8 digits and \n, \0 at the end */
	char buf[MEI_FW_STATUS_MAX * 9 + 1] = {0};
	char *nptr, *endptr;

	if (!fw_status)
		return -1;

	offset = snprintf(path, sizeof(path) - 1, "%s/%s/%s",
			  MEI_SYSFS_ROOT, vmei->name, "fw_status");
	if (offset < 0)
		return -1;

	if (mei_sysfs_read_property_file(path, buf, sizeof(buf) - 1) < 0)
		return -1;

	nptr = buf;
	for (i = 0; i < MEI_FW_STATUS_MAX; i++) {
		res = strtoul(nptr, &endptr, 16);
		if (res == ULONG_MAX) {
			fw_status[i] = 0;
			continue;
		}
		if (nptr == endptr) {
			fw_status[i] = 0;
			continue;
		}
		fw_status[i] = res;
		nptr = endptr;
	}

	return 0;
}

static void
vmei_virtual_fw_reset(struct virtio_mei *vmei)
{
	DPRINTF("Firmware reset\n");
	struct vmei_me_client *e;

	vmei_set_status(vmei, VMEI_STS_RESET);
	vmei->config->hw_ready = 0;
	close(vmei->hbm_fd);

	/* disconnect all */
	pthread_mutex_lock(&vmei->list_mutex);
	LIST_FOREACH(e, &vmei->active_clients, list) {
		vmei_me_client_destroy_host_clients(e);
	}
	pthread_mutex_unlock(&vmei->list_mutex);
}

static void
vmei_reset(void *vth)
{
	struct virtio_mei *vmei = vth;

	DPRINTF("device reset requested!\n");
	vmei_virtual_fw_reset(vmei);
	virtio_reset_dev(&vmei->base);
}

static void vmei_del_reset_event(struct virtio_mei *vmei)
{
	if (vmei->reset_mevp)
		mevent_delete_close(vmei->reset_mevp);

	vmei->reset_mevp = NULL;
}

static void
vmei_reset_callback(int fd, enum ev_type type, void *param)
{
	static bool first_time = true;
	struct virtio_mei *vmei = param;
	char buf[MEI_DEV_STATE_LEN] = {0};
	int sz;

	lseek(fd, 0, SEEK_SET);
	sz = read(fd, buf, 12);
	if (first_time) {
		first_time = false;
		return;
	}

	if (sz != 7 || memcmp(buf, "ENABLED", 7))
		return;

	DPRINTF("Reset state callback\n");

	vmei_virtual_fw_reset(vmei);
	vmei_free_me_clients(vmei);
	vmei_start(vmei, true);
}

static int vmei_add_reset_event(struct virtio_mei *vmei)
{
	char devpath[256];
	int dev_state_fd;

	snprintf(devpath, sizeof(devpath) - 1, "%s/%s/%s",
		 MEI_SYSFS_ROOT, vmei->name, "dev_state");

	dev_state_fd = open(devpath, O_RDONLY);
	if (dev_state_fd < 0)
		return -errno;

	vmei->reset_mevp = mevent_add(dev_state_fd, EVF_READ_ET,
				      vmei_reset_callback, vmei);
	if (!vmei->reset_mevp) {
		close(dev_state_fd);
		return -ENOMEM;
	}
	return 0;
}

static inline uint8_t
errno_to_hbm_conn_status(int err)
{
	switch (err) {
	case 0:		return MEI_CL_CONN_SUCCESS;
	case ENOTTY:	return MEI_CL_CONN_NOT_FOUND;
	case EBUSY:	return MEI_CL_CONN_ALREADY_STARTED;
	case EINVAL:	return MEI_HBM_INVALID_PARAMETER;
	default:	return MEI_HBM_INVALID_PARAMETER;
	}
}

/**
 * vmei_host_client_native_connect() connect host client
 *    to the native driver and register with mevent.
 *
 * @hclient: host client instance
 * Return: HBM status cot_client
 */
static uint8_t
vmei_host_client_native_connect(struct vmei_host_client *hclient)
{
	struct virtio_mei *vmei = vmei_host_client_to_vmei(hclient);
	struct vmei_me_client *mclient;
	struct mei_connect_client_data_vtag connection_data;

	mclient = hclient->mclient;

	/* open mei node */
	hclient->client_fd = open(vmei->devnode, O_RDWR | O_NONBLOCK);
	if (hclient->client_fd < 0) {
		HCL_WARN(hclient, "open %s failed %d\n", vmei->devnode, errno);
		return MEI_HBM_REJECTED;
	}

	memset(&connection_data, 0, sizeof(connection_data));
	memcpy(&connection_data.connect.in_client_uuid,
	       &mclient->props.protocol_name,
	       sizeof(connection_data.connect.in_client_uuid));
	connection_data.connect.vtag  = vmei->vtag;

	if (ioctl(hclient->client_fd, IOCTL_MEI_CONNECT_CLIENT_VTAG,
		  &connection_data) == -1) {
		HCL_DBG(hclient, "connect failed %d!\n", errno);
		return errno_to_hbm_conn_status(errno);
	}

	/* add READ event into mevent */
	hclient->rx_mevp = mevent_add(hclient->client_fd, EVF_READ,
				      vmei_rx_callback, hclient);
	if (!hclient->rx_mevp)
		return MEI_HBM_REJECTED;

	if (!hclient->recv_creds)
		mevent_disable(hclient->rx_mevp);

	HCL_DBG(hclient, "connect succeeded!\n");

	return MEI_HBM_SUCCESS;
}

static void mei_set_msg_hdr(struct vmei_host_client *hclient,
			    struct mei_msg_hdr *hdr, uint32_t len,
			    bool complete)
{
	if (!hdr)
		return;

	memset(hdr, 0, sizeof(*hdr));
	hdr->me_addr      = hclient->me_addr;
	hdr->host_addr    = hclient->host_addr;
	hdr->length       = len;
	hdr->msg_complete = complete ? 1 : 0;
}

static int
vmei_hbm_response(struct virtio_mei *vmei, void *data, int len)
{
	return write(vmei->hbm_fd, data, len);
}

/*
 * This function is intend to send HBM disconnection command to guest.
 */
static int
vmei_hbm_disconnect_client(struct vmei_host_client *hclient)
{
	struct virtio_mei *vmei = vmei_host_client_to_vmei(hclient);
	struct mei_hbm_client_disconnect_req disconnect_req;

	if (!vmei)
		return -1;

	disconnect_req.hbm_cmd.cmd = MEI_HBM_CLIENT_DISCONNECT;
	disconnect_req.me_addr = hclient->me_addr;
	disconnect_req.host_addr = hclient->host_addr;

	HCL_DBG(hclient, "DM->UOS: Disconnect Client\n");

	return vmei_hbm_response(vmei, &disconnect_req, sizeof(disconnect_req));
}

/**
 * vmei_hbm_flow_ctl_req() - send flow control message to MEI-FE
 *
 * @hclient: host client
 *
 * Return: 0 on success
 */
static int
vmei_hbm_flow_ctl_req(struct vmei_host_client *hclient)
{
	struct virtio_mei *vmei = vmei_host_client_to_vmei(hclient);
	struct mei_hbm_flow_ctl flow_ctl_req;

	if (!vmei)
		return -1;

	if (!hclient->host_addr)
		return 0;

	memset(&flow_ctl_req, 0, sizeof(flow_ctl_req));
	flow_ctl_req.hbm_cmd.cmd = MEI_HBM_FLOW_CONTROL;
	flow_ctl_req.me_addr = hclient->me_addr;
	flow_ctl_req.host_addr = hclient->host_addr;

	HCL_DBG(hclient, "DM->UOS: Flow Control\n");
	return vmei_hbm_response(vmei, &flow_ctl_req, sizeof(flow_ctl_req));
}

static int
vmei_hbm_version(struct virtio_mei *vmei,
		 const struct mei_hbm_host_ver_req *ver_req)
{
	const struct {
		uint8_t major;
		uint8_t minor;
	} supported_vers[] = {
		{ VMEI_HBM_VER_MAJ, VMEI_HBM_VER_MIN },
		{ 1,                1 },
		{ 1,                0 }
	};
	unsigned int i;
	struct mei_hbm_host_ver_res ver_res;

	ver_res.hbm_cmd.cmd = MEI_HBM_HOST_START;
	ver_res.hbm_cmd.is_response = 1;

	ver_res.minor = VMEI_HBM_VER_MIN;
	ver_res.major = VMEI_HBM_VER_MAJ;
	ver_res.host_ver_support = 0;

	for (i = 0; i < ARRAY_SIZE(supported_vers); i++) {
		if (supported_vers[i].major == ver_req->major &&
		    supported_vers[i].minor == ver_req->minor)
			ver_res.host_ver_support = 1;
	}

	return vmei_hbm_response(vmei, &ver_res, sizeof(ver_res));
}

static void
vmei_hbm_handler(struct virtio_mei *vmei, const void *data)
{
	struct mei_hbm_cmd *hbm_cmd = (struct mei_hbm_cmd *)data;
	struct vmei_me_client *mclient = NULL;
	struct vmei_host_client *hclient = NULL;

	uint8_t status;

	struct mei_hbm_host_enum_req *enum_req;
	struct mei_hbm_host_enum_res enum_res;
	struct mei_hbm_host_client_prop_req *client_prop_req;
	struct mei_hbm_host_client_prop_res client_prop_res;
	struct mei_hbm_client_connect_req *connect_req;
	struct mei_hbm_client_connect_res connect_res;
	struct mei_hbm_client_disconnect_req *disconnect_req;
	struct mei_hbm_client_disconnect_res disconnect_res;
	struct mei_hbm_flow_ctl *flow_ctl_req;
	struct mei_hbm_power_gate pgi_res;
	struct mei_hbm_notification_req *notif_req;
	struct mei_hbm_notification_res notif_res;

	DPRINTF("HBM cmd[0x%X] is handling\n", hbm_cmd->cmd);

	switch (hbm_cmd->cmd) {
	case MEI_HBM_HOST_START:
		vmei_hbm_version(vmei, data);
		break;

	case MEI_HBM_HOST_ENUM:
		enum_req = (struct mei_hbm_host_enum_req *)data;

		enum_res.hbm_cmd = enum_req->hbm_cmd;
		enum_res.hbm_cmd.is_response = 1;
		/* copy valid_addresses for hbm enum request */
		memcpy(enum_res.valid_addresses, &vmei->me_clients_map,
		       sizeof(enum_res.valid_addresses));
		vmei_hbm_response(vmei, &enum_res, sizeof(enum_res));

		break;

	case MEI_HBM_HOST_CLIENT_PROP:
		client_prop_req = (struct mei_hbm_host_client_prop_req *)data;

		client_prop_res.hbm_cmd = client_prop_req->hbm_cmd;
		client_prop_res.hbm_cmd.is_response = 1;
		client_prop_res.address = client_prop_req->address;

		mclient = vmei_find_me_client(vmei, client_prop_req->address);
		if (mclient) {
			memcpy(&client_prop_res.props, &mclient->props,
			       sizeof(client_prop_res.props));
			client_prop_res.status = MEI_HBM_SUCCESS;
		} else {
			client_prop_res.status = MEI_HBM_CLIENT_NOT_FOUND;
		}

		vmei_dbg_client_properties(&client_prop_res.props);

		vmei_hbm_response(vmei,
				  &client_prop_res,
				  sizeof(client_prop_res));
		break;

	case MEI_HBM_FLOW_CONTROL:
		/*
		 * FE client is ready, we can send message
		 */
		flow_ctl_req = (struct mei_hbm_flow_ctl *)data;
		hclient = vmei_find_host_client(vmei,
						flow_ctl_req->me_addr,
						flow_ctl_req->host_addr);
		if (hclient) {
			hclient->recv_creds++;
			mevent_enable(hclient->rx_mevp);

			vmei_host_client_put(hclient);

			pthread_mutex_lock(&vmei->rx_mutex);
			vmei->rx_need_sched = true;
			pthread_mutex_unlock(&vmei->rx_mutex);
		} else {
			DPRINTF("client has been released.\n");
		}
		break;

	case MEI_HBM_CLIENT_CONNECT:

		connect_req = (struct mei_hbm_client_connect_req *)data;

		connect_res = *(struct mei_hbm_client_connect_res *)connect_req;
		connect_res.hbm_cmd.is_response = 1;

		mclient = vmei_find_me_client(vmei, connect_req->me_addr);
		if (!mclient) {
			/* NOT FOUND */
			DPRINTF("client with me address %d was not found\n",
				connect_req->me_addr);
			connect_res.status = MEI_HBM_CLIENT_NOT_FOUND;
			vmei_hbm_response(vmei, &connect_res,
					  sizeof(connect_res));
			break;
		}

		hclient = vmei_me_client_get_host_client(mclient,
							 connect_req->host_addr);
		if (hclient) {
			DPRINTF("client alread exists return BUSY!\n");
			connect_res.status = MEI_HBM_ALREADY_EXISTS;
			vmei_host_client_put(hclient);
			hclient = NULL;
			vmei_hbm_response(vmei, &connect_res,
					  sizeof(connect_res));
			break;
		}

		/* create a new host client and add ito the list */
		hclient = vmei_host_client_create(mclient,
						  connect_req->host_addr);
		if (!hclient) {
			connect_res.status = MEI_HBM_REJECTED;
			vmei_hbm_response(vmei, &connect_res,
					  sizeof(connect_res));
			break;
		}

		status = vmei_host_client_native_connect(hclient);
		connect_res.status = status;
		vmei_hbm_response(vmei, &connect_res, sizeof(connect_res));

		if (status) {
			vmei_host_client_put(hclient);
			hclient = NULL;
			break;
		}

		vmei_hbm_flow_ctl_req(hclient);

		break;

	case MEI_HBM_CLIENT_DISCONNECT:

		disconnect_req = (struct mei_hbm_client_disconnect_req *)data;
		hclient = vmei_find_host_client(vmei, disconnect_req->me_addr,
						disconnect_req->host_addr);
		if (hclient) {
			vmei_host_client_put(hclient);
			vmei_host_client_put(hclient);
			hclient = NULL;
			status = MEI_HBM_SUCCESS;
		} else {
			status = MEI_HBM_CLIENT_NOT_FOUND;
		}

		if (hbm_cmd->is_response)
			break;

		memset(&disconnect_res, 0, sizeof(disconnect_res));
		disconnect_res.hbm_cmd.cmd = MEI_HBM_CLIENT_DISCONNECT;
		disconnect_res.hbm_cmd.is_response = 1;
		disconnect_res.status = status;
		vmei_hbm_response(vmei, &disconnect_res,
				  sizeof(disconnect_res));

		break;

	case MEI_HBM_PG_ISOLATION_ENTRY:

		memset(&pgi_res, 0, sizeof(pgi_res));
		pgi_res.hbm_cmd.cmd = MEI_HBM_PG_ISOLATION_ENTRY;
		pgi_res.hbm_cmd.is_response = 1;
		vmei_hbm_response(vmei, &pgi_res, sizeof(pgi_res));

		break;

	case MEI_HBM_NOTIFY:

		notif_req = (struct mei_hbm_notification_req *)data;
		memset(&notif_res, 0, sizeof(notif_res));
		notif_res.hbm_cmd.cmd = MEI_HBM_NOTIFY;
		notif_res.hbm_cmd.is_response = 1;
		notif_res.start = notif_req->start;

		hclient = vmei_find_host_client(vmei, notif_req->me_addr,
						notif_req->host_addr);
		if (hclient) {
			/* The notify is not supported now */
			notif_res.status = MEI_HBM_REJECTED;
			vmei_host_client_put(hclient);
		} else {
			notif_res.status = MEI_HBM_INVALID_PARAMETER;
		}
		vmei_hbm_response(vmei, &notif_res, sizeof(notif_res));

		break;

	case MEI_HBM_HOST_STOP:
		DPRINTF("HBM cmd[%d] not supported\n", hbm_cmd->cmd);
		break;

	case MEI_HBM_ME_STOP:
		DPRINTF("HBM cmd[%d] not supported\n", hbm_cmd->cmd);
		break;

	default:
		DPRINTF("HBM cmd[0x%X] unhandled\n", hbm_cmd->cmd);
	}
}

static ssize_t
vmei_host_client_native_write(struct vmei_host_client *hclient)
{
	struct virtio_mei *vmei = vmei_host_client_to_vmei(hclient);
	ssize_t len, lencnt = 0;
	int err;
	unsigned int iovcnt, i;
	struct vmei_circular_iobufs *bufs = &hclient->send_bufs;

	if (!vmei)
		return -EINVAL;

	if (hclient->client_fd < 0) {
		HCL_WARN(hclient, "invalid client fd [%d]\n",
			 hclient->client_fd);
		return -EINVAL;
	}

	if (bufs->i_idx == bufs->r_idx) {
		/* nothing to send actually */
		WPRINTF("no buffer to send\n");
		return 0;
	}

	while (bufs->i_idx != bufs->r_idx) {
		/*
		 * calculate number of entries
		 * while taking care of wraparound
		 */
		iovcnt = (bufs->i_idx > bufs->r_idx) ?
			 (bufs->i_idx - bufs->r_idx) :
			 (VMEI_IOBUFS_MAX - bufs->r_idx);

		for (i = 0; i < iovcnt; i++) {
			len = writev(hclient->client_fd,
				     &bufs->bufs[bufs->r_idx + i],
				     1);
			if (len < 0) {
				err = -errno;
				if (err != -EAGAIN)
					WPRINTF("write failed! error[%d]\n",
						errno);
				if (err == -ENODEV)
					vmei_set_status(vmei,
							VMEI_STS_PENDING_RESET);
				return -errno;
			}

			lencnt += len;

			bufs->bufs[bufs->r_idx + i].iov_len = 0;
			bufs->complete[bufs->r_idx + i] = 0;
		}

		bufs->r_idx = (bufs->i_idx > bufs->r_idx) ? bufs->i_idx : 0;
	}

	return lencnt;
}

/*
 * A completed read guarantees that a client message is completed,
 * transmission of client message is started by a flow control message of HBM
 */
static ssize_t
vmei_host_client_native_read(struct vmei_host_client *hclient)
{
	struct virtio_mei *vmei = vmei_host_client_to_vmei(hclient);
	ssize_t len;

	if (hclient->client_fd < 0) {
		HCL_WARN(hclient, "RX: invalid client fd\n");
		return -1;
	}

	/* read with max_message_length */
	len = read(hclient->client_fd, hclient->recv_buf, hclient->recv_buf_sz);
	if (len < 0) {
		HCL_WARN(hclient, "RX: read failed! read error[%d]\n", errno);
		if (errno == ENODEV)
			vmei_set_status(vmei, VMEI_STS_PENDING_RESET);
		return len;
	}

	HCL_DBG(hclient, "RX: append data len=%zd at off=%d\n",
		len, hclient->recv_offset);

	hclient->recv_offset = len;

	return hclient->recv_offset;
}

static int
vmei_cfgread(void *vsc, int offset, int size, uint32_t *retval)
{
	struct virtio_mei *vmei = vsc;
	void *ptr;

	if (offset + size >= (int)offsetof(struct mei_virtio_cfg, fw_status)) {
		if (vmei_read_fw_status(vmei, vmei->config->fw_status) < 0)
			return -1;
	}

	ptr = (uint8_t *)vmei->config + offset;
	memcpy(retval, ptr, size);
	DPRINTF("fw_status[%d] = 0x%08x\n", (offset / size) - 2, *retval);
	return 0;
}

static int
vmei_cfgwrite(void *vsc, int offset, int size, uint32_t val)
{
	struct virtio_mei *vmei = vsc;

	DPRINTF("cfgwrite: offset = %d, size = %d val = %d\n",
		offset, size, val);

	if (offset != offsetof(struct mei_virtio_cfg, host_reset)) {
		WPRINTF("cfgwrite: not a reset\n");
		return 0;
	}

	if (size == sizeof(uint8_t) && val == 1) {
		DPRINTF("cfgwrite: host_reset [%d]\n", val);
		/* guest initate reset need restart */

		vmei_virtual_fw_reset(vmei);
		virtio_config_changed(&vmei->base);
	}

	if (size == sizeof(uint8_t) && val == 0) {
		DPRINTF("cfgwrite: host_reset_release [%d]\n", val);
		/* guest initate reset need restart */

		virtio_config_changed(&vmei->base);
	}

	return 0;
}

