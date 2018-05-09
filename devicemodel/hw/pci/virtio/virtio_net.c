/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#ifndef NETMAP_WITH_LIBS
#define NETMAP_WITH_LIBS
#endif
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>
#include <openssl/md5.h>
#include <pthread.h>
#include <sysexits.h>

#include "types.h"
#include "dm.h"
#include "pci_core.h"
#include "mevent.h"
#include "virtio.h"
#include "netmap_user.h"
#include <net/if.h>
#include <linux/if_tun.h>

#define VIRTIO_NET_RINGSZ	1024
#define VIRTIO_NET_MAXSEGS	256

/*
 * Host capabilities.  Note that we only offer a few of these.
 */
#define	VIRTIO_NET_F_CSUM	(1 <<  0) /* host handles partial cksum */
#define	VIRTIO_NET_F_GUEST_CSUM	(1 <<  1) /* guest handles partial cksum */
#define	VIRTIO_NET_F_MAC	(1 <<  5) /* host supplies MAC */
#define	VIRTIO_NET_F_GSO_DEPREC	(1 <<  6) /* deprecated: host handles GSO */
#define	VIRTIO_NET_F_GUEST_TSO4	(1 <<  7) /* guest can rcv TSOv4 */
#define	VIRTIO_NET_F_GUEST_TSO6	(1 <<  8) /* guest can rcv TSOv6 */
#define	VIRTIO_NET_F_GUEST_ECN	(1 <<  9) /* guest can rcv TSO with ECN */
#define	VIRTIO_NET_F_GUEST_UFO	(1 << 10) /* guest can rcv UFO */
#define	VIRTIO_NET_F_HOST_TSO4	(1 << 11) /* host can rcv TSOv4 */
#define	VIRTIO_NET_F_HOST_TSO6	(1 << 12) /* host can rcv TSOv6 */
#define	VIRTIO_NET_F_HOST_ECN	(1 << 13) /* host can rcv TSO with ECN */
#define	VIRTIO_NET_F_HOST_UFO	(1 << 14) /* host can rcv UFO */
#define	VIRTIO_NET_F_MRG_RXBUF	(1 << 15) /* host can merge RX buffers */
#define	VIRTIO_NET_F_STATUS	(1 << 16) /* config status field available */
#define	VIRTIO_NET_F_CTRL_VQ	(1 << 17) /* control channel available */
#define	VIRTIO_NET_F_CTRL_RX	(1 << 18) /* control channel RX mode support */
#define	VIRTIO_NET_F_CTRL_VLAN	(1 << 19) /* control channel VLAN filtering */
#define	VIRTIO_NET_F_GUEST_ANNOUNCE \
				(1 << 21) /* guest can send gratuitous pkts */

#define VIRTIO_NET_S_HOSTCAPS      \
	(VIRTIO_NET_F_MAC | VIRTIO_NET_F_MRG_RXBUF | VIRTIO_NET_F_STATUS | \
	VIRTIO_F_NOTIFY_ON_EMPTY | VIRTIO_RING_F_INDIRECT_DESC)

/* is address mcast/bcast? */
#define ETHER_IS_MULTICAST(addr) (*(addr) & 0x01)

/*
 * PCI config-space "registers"
 */
struct virtio_net_config {
	uint8_t  mac[6];
	uint16_t status;
} __attribute__((packed));

/*
 * Queue definitions.
 */
#define VIRTIO_NET_RXQ	0
#define VIRTIO_NET_TXQ	1
#define VIRTIO_NET_CTLQ	2	/* NB: not yet supported */

#define VIRTIO_NET_MAXQ	3

/*
 * Fixed network header size
 */
struct virtio_net_rxhdr {
	uint8_t		vrh_flags;
	uint8_t		vrh_gso_type;
	uint16_t	vrh_hdr_len;
	uint16_t	vrh_gso_size;
	uint16_t	vrh_csum_start;
	uint16_t	vrh_csum_offset;
	uint16_t	vrh_bufs;
} __attribute__((packed));

/*
 * Debug printf
 */
static int virtio_net_debug;
#define DPRINTF(params) do { if (virtio_net_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

/*
 * Per-device struct
 */
struct virtio_net {
	struct virtio_base base;
	struct virtio_vq_info queues[VIRTIO_NET_MAXQ - 1];
	pthread_mutex_t mtx;
	struct mevent	*mevp;

	int		tapfd;
	struct nm_desc	*nmd;

	int		rx_ready;

	volatile int	resetting;	/* set and checked outside lock */
	volatile int	closing;	/* stop the tx i/o thread */

	uint64_t	features;	/* negotiated features */

	struct virtio_net_config config;

	pthread_mutex_t	rx_mtx;
	int		rx_in_progress;
	int		rx_vhdrlen;
	int		rx_merge;	/* merged rx bufs in use */
	pthread_t	tx_tid;
	pthread_mutex_t	tx_mtx;
	pthread_cond_t	tx_cond;
	int		tx_in_progress;

	void (*virtio_net_rx)(struct virtio_net *net);
	void (*virtio_net_tx)(struct virtio_net *net, struct iovec *iov,
			     int iovcnt, int len);
};

static void virtio_net_reset(void *);
static void virtio_net_tx_stop(struct virtio_net *);
/* static void virtio_net_notify(void *, struct virtio_vq_info *); */
static int virtio_net_cfgread(void *, int, int, uint32_t *);
static int virtio_net_cfgwrite(void *, int, int, uint32_t);
static void virtio_net_neg_features(void *, uint64_t);

static struct virtio_ops virtio_net_ops = {
	"vtnet",			/* our name */
	VIRTIO_NET_MAXQ - 1,		/* we currently support 2 virtqueues */
	sizeof(struct virtio_net_config), /* config reg size */
	virtio_net_reset,		/* reset */
	NULL,				/* device-wide qnotify -- not used */
	virtio_net_cfgread,		/* read PCI config */
	virtio_net_cfgwrite,		/* write PCI config */
	virtio_net_neg_features,	/* apply negotiated features */
	NULL,				/* called on guest set status */
	VIRTIO_NET_S_HOSTCAPS,		/* our capabilities */
};

static struct ether_addr *
ether_aton(const char *a, struct ether_addr *e)
{
	int i;
	unsigned int o0, o1, o2, o3, o4, o5;

	i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o0, &o1, &o2, &o3, &o4, &o5);
	if (i != 6)
		return NULL;
	e->ether_addr_octet[0] = o0;
	e->ether_addr_octet[1] = o1;
	e->ether_addr_octet[2] = o2;
	e->ether_addr_octet[3] = o3;
	e->ether_addr_octet[4] = o4;
	e->ether_addr_octet[5] = o5;
	return e;
}

/*
 * If the transmit thread is active then stall until it is done.
 */
static void
virtio_net_txwait(struct virtio_net *net)
{
	pthread_mutex_lock(&net->tx_mtx);
	while (net->tx_in_progress) {
		pthread_mutex_unlock(&net->tx_mtx);
		usleep(10000);
		pthread_mutex_lock(&net->tx_mtx);
	}
	pthread_mutex_unlock(&net->tx_mtx);
}

/*
 * If the receive thread is active then stall until it is done.
 */
static void
virtio_net_rxwait(struct virtio_net *net)
{
	pthread_mutex_lock(&net->rx_mtx);
	while (net->rx_in_progress) {
		pthread_mutex_unlock(&net->rx_mtx);
		usleep(10000);
		pthread_mutex_lock(&net->rx_mtx);
	}
	pthread_mutex_unlock(&net->rx_mtx);
}

static void
virtio_net_reset(void *vdev)
{
	struct virtio_net *net = vdev;

	DPRINTF(("vtnet: device reset requested !\n"));

	net->resetting = 1;

	/*
	 * Wait for the transmit and receive threads to finish their
	 * processing.
	 */
	virtio_net_txwait(net);
	virtio_net_rxwait(net);

	net->rx_ready = 0;
	net->rx_merge = 1;
	net->rx_vhdrlen = sizeof(struct virtio_net_rxhdr);

	/* now reset rings, MSI-X vectors, and negotiated capabilities */
	virtio_reset_dev(&net->base);

	net->resetting = 0;
	net->closing = 0;
}

/*
 * Send signal to tx I/O thread and wait till it exits
 */
static void
virtio_net_tx_stop(struct virtio_net *net)
{
	void *jval;

	net->closing = 1;

	pthread_cond_broadcast(&net->tx_cond);

	pthread_join(net->tx_tid, &jval);
}

/*
 * Called to send a buffer chain out to the tap device
 */
static void
virtio_net_tap_tx(struct virtio_net *net, struct iovec *iov, int iovcnt,
		  int len)
{
	static char pad[60]; /* all zero bytes */
	ssize_t ret;

	if (net->tapfd == -1)
		return;

	/*
	 * If the length is < 60, pad out to that and add the
	 * extra zero'd segment to the iov. It is guaranteed that
	 * there is always an extra iov available by the caller.
	 */
	if (len < 60) {
		iov[iovcnt].iov_base = pad;
		iov[iovcnt].iov_len = 60 - len;
		iovcnt++;
	}
	ret = writev(net->tapfd, iov, iovcnt);
	(void)ret; /*avoid compiler warning*/
}

/*
 *  Called when there is read activity on the tap file descriptor.
 * Each buffer posted by the guest is assumed to be able to contain
 * an entire ethernet frame + rx header.
 *  MP note: the dummybuf is only used for discarding frames, so there
 * is no need for it to be per-vtnet or locked.
 */
static uint8_t dummybuf[2048];

static inline struct iovec *
rx_iov_trim(struct iovec *iov, int *niov, int tlen)
{
	struct iovec *riov;

	/* XXX short-cut: assume first segment is >= tlen */
	assert(iov[0].iov_len >= tlen);

	iov[0].iov_len -= tlen;
	if (iov[0].iov_len == 0) {
		assert(*niov > 1);
		*niov -= 1;
		riov = &iov[1];
	} else {
		iov[0].iov_base = (void *)((uintptr_t)iov[0].iov_base + tlen);
		riov = &iov[0];
	}

	return riov;
}

static void
virtio_net_tap_rx(struct virtio_net *net)
{
	struct iovec iov[VIRTIO_NET_MAXSEGS], *riov;
	struct virtio_vq_info *vq;
	void *vrx;
	int len, n;
	uint16_t idx;
	ssize_t ret;

	/*
	 * Should never be called without a valid tap fd
	 */
	assert(net->tapfd != -1);

	/*
	 * But, will be called when the rx ring hasn't yet
	 * been set up or the guest is resetting the device.
	 */
	if (!net->rx_ready || net->resetting) {
		/*
		 * Drop the packet and try later.
		 */
		ret = read(net->tapfd, dummybuf, sizeof(dummybuf));
		(void)ret; /*avoid compiler warning*/

		return;
	}

	/*
	 * Check for available rx buffers
	 */
	vq = &net->queues[VIRTIO_NET_RXQ];
	if (!vq_has_descs(vq)) {
		/*
		 * Drop the packet and try later.  Interrupt on
		 * empty, if that's negotiated.
		 */
		ret = read(net->tapfd, dummybuf, sizeof(dummybuf));
		(void)ret; /*avoid compiler warning*/

		vq_endchains(vq, 1);
		return;
	}

	do {
		/*
		 * Get descriptor chain.
		 */
		n = vq_getchain(vq, &idx, iov, VIRTIO_NET_MAXSEGS, NULL);
		assert(n >= 1 && n <= VIRTIO_NET_MAXSEGS);

		/*
		 * Get a pointer to the rx header, and use the
		 * data immediately following it for the packet buffer.
		 */
		vrx = iov[0].iov_base;
		riov = rx_iov_trim(iov, &n, net->rx_vhdrlen);

		len = readv(net->tapfd, riov, n);

		if (len < 0 && errno == EWOULDBLOCK) {
			/*
			 * No more packets, but still some avail ring
			 * entries.  Interrupt if needed/appropriate.
			 */
			vq_retchain(vq);
			vq_endchains(vq, 0);
			return;
		}

		/*
		 * The only valid field in the rx packet header is the
		 * number of buffers if merged rx bufs were negotiated.
		 */
		memset(vrx, 0, net->rx_vhdrlen);

		if (net->rx_merge) {
			struct virtio_net_rxhdr *vrxh;

			vrxh = vrx;
			vrxh->vrh_bufs = 1;
		}

		/*
		 * Release this chain and handle more chains.
		 */
		vq_relchain(vq, idx, len + net->rx_vhdrlen);
	} while (vq_has_descs(vq));

	/* Interrupt if needed, including for NOTIFY_ON_EMPTY. */
	vq_endchains(vq, 1);
}

static inline int
virtio_net_netmap_writev(struct nm_desc *nmd, struct iovec *iov, int iovcnt)
{
	int r, i;
	int len = 0;

	for (r = nmd->cur_tx_ring; ; ) {
		struct netmap_ring *ring = NETMAP_TXRING(nmd->nifp, r);
		uint32_t cur, idx;
		char *buf;

		if (nm_ring_empty(ring)) {
			r++;
			if (r > nmd->last_tx_ring)
				r = nmd->first_tx_ring;
			if (r == nmd->cur_tx_ring)
				break;
			continue;
		}
		cur = ring->cur;
		idx = ring->slot[cur].buf_idx;
		buf = NETMAP_BUF(ring, idx);

		for (i = 0; i < iovcnt; i++) {
			if (len + iov[i].iov_len > 2048)
				break;
			memcpy(&buf[len], iov[i].iov_base, iov[i].iov_len);
			len += iov[i].iov_len;
		}
		ring->slot[cur].len = len;
		ring->head = ring->cur = nm_ring_next(ring, cur);
		nmd->cur_tx_ring = r;
		ioctl(nmd->fd, NIOCTXSYNC, NULL);
		break;
	}

	return len;
}

static inline int
virtio_net_netmap_readv(struct nm_desc *nmd, struct iovec *iov, int iovcnt)
{
	int len = 0;
	int i = 0;
	int r;

	for (r = nmd->cur_rx_ring; ; ) {
		struct netmap_ring *ring = NETMAP_RXRING(nmd->nifp, r);
		uint32_t cur, idx;
		char *buf;
		size_t left;

		if (nm_ring_empty(ring)) {
			r++;
			if (r > nmd->last_rx_ring)
				r = nmd->first_rx_ring;
			if (r == nmd->cur_rx_ring)
				break;
			continue;
		}
		cur = ring->cur;
		idx = ring->slot[cur].buf_idx;
		buf = NETMAP_BUF(ring, idx);
		left = ring->slot[cur].len;

		for (i = 0; i < iovcnt && left > 0; i++) {
			if (iov[i].iov_len > left)
				iov[i].iov_len = left;
			memcpy(iov[i].iov_base, &buf[len], iov[i].iov_len);
			len += iov[i].iov_len;
			left -= iov[i].iov_len;
		}
		ring->head = ring->cur = nm_ring_next(ring, cur);
		nmd->cur_rx_ring = r;
		ioctl(nmd->fd, NIOCRXSYNC, NULL);
		break;
	}
	for (; i < iovcnt; i++)
		iov[i].iov_len = 0;

	return len;
}

/*
 * Called to send a buffer chain out to the vale port
 */
static void
virtio_net_netmap_tx(struct virtio_net *net, struct iovec *iov, int iovcnt,
		    int len)
{
	static char pad[60]; /* all zero bytes */

	if (net->nmd == NULL)
		return;

	/*
	 * If the length is < 60, pad out to that and add the
	 * extra zero'd segment to the iov. It is guaranteed that
	 * there is always an extra iov available by the caller.
	 */
	if (len < 60) {
		iov[iovcnt].iov_base = pad;
		iov[iovcnt].iov_len = 60 - len;
		iovcnt++;
	}
	(void) virtio_net_netmap_writev(net->nmd, iov, iovcnt);
}

static void
virtio_net_netmap_rx(struct virtio_net *net)
{
	struct iovec iov[VIRTIO_NET_MAXSEGS], *riov;
	struct virtio_vq_info *vq;
	void *vrx;
	int len, n;
	uint16_t idx;

	/*
	 * Should never be called without a valid netmap descriptor
	 */
	assert(net->nmd != NULL);

	/*
	 * But, will be called when the rx ring hasn't yet
	 * been set up or the guest is resetting the device.
	 */
	if (!net->rx_ready || net->resetting) {
		/*
		 * Drop the packet and try later.
		 */
		(void) nm_nextpkt(net->nmd, (void *)dummybuf);
		return;
	}

	/*
	 * Check for available rx buffers
	 */
	vq = &net->queues[VIRTIO_NET_RXQ];
	if (!vq_has_descs(vq)) {
		/*
		 * Drop the packet and try later.  Interrupt on
		 * empty, if that's negotiated.
		 */
		(void) nm_nextpkt(net->nmd, (void *)dummybuf);
		vq_endchains(vq, 1);
		return;
	}

	do {
		/*
		 * Get descriptor chain.
		 */
		n = vq_getchain(vq, &idx, iov, VIRTIO_NET_MAXSEGS, NULL);
		assert(n >= 1 && n <= VIRTIO_NET_MAXSEGS);

		/*
		 * Get a pointer to the rx header, and use the
		 * data immediately following it for the packet buffer.
		 */
		vrx = iov[0].iov_base;
		riov = rx_iov_trim(iov, &n, net->rx_vhdrlen);

		len = virtio_net_netmap_readv(net->nmd, riov, n);

		if (len == 0) {
			/*
			 * No more packets, but still some avail ring
			 * entries.  Interrupt if needed/appropriate.
			 */
			vq_retchain(vq);
			vq_endchains(vq, 0);
			return;
		}

		/*
		 * The only valid field in the rx packet header is the
		 * number of buffers if merged rx bufs were negotiated.
		 */
		memset(vrx, 0, net->rx_vhdrlen);

		if (net->rx_merge) {
			struct virtio_net_rxhdr *vrxh;

			vrxh = vrx;
			vrxh->vrh_bufs = 1;
		}

		/*
		 * Release this chain and handle more chains.
		 */
		vq_relchain(vq, idx, len + net->rx_vhdrlen);
	} while (vq_has_descs(vq));

	/* Interrupt if needed, including for NOTIFY_ON_EMPTY. */
	vq_endchains(vq, 1);
}

static void
virtio_net_rx_callback(int fd, enum ev_type type, void *param)
{
	struct virtio_net *net = param;

	pthread_mutex_lock(&net->rx_mtx);
	net->rx_in_progress = 1;
	net->virtio_net_rx(net);
	net->rx_in_progress = 0;
	pthread_mutex_unlock(&net->rx_mtx);

}

static void
virtio_net_ping_rxq(void *vdev, struct virtio_vq_info *vq)
{
	struct virtio_net *net = vdev;

	/*
	 * A qnotify means that the rx process can now begin
	 */
	if (net->rx_ready == 0) {
		net->rx_ready = 1;
		vq->used->flags |= VRING_USED_F_NO_NOTIFY;
	}
}

static void
virtio_net_proctx(struct virtio_net *net, struct virtio_vq_info *vq)
{
	struct iovec iov[VIRTIO_NET_MAXSEGS + 1];
	int i, n;
	int plen, tlen;
	uint16_t idx;

	/*
	 * Obtain chain of descriptors.  The first one is
	 * really the header descriptor, so we need to sum
	 * up two lengths: packet length and transfer length.
	 */
	n = vq_getchain(vq, &idx, iov, VIRTIO_NET_MAXSEGS, NULL);
	assert(n >= 1 && n <= VIRTIO_NET_MAXSEGS);
	plen = 0;
	tlen = iov[0].iov_len;
	for (i = 1; i < n; i++) {
		plen += iov[i].iov_len;
		tlen += iov[i].iov_len;
	}

	DPRINTF(("virtio: packet send, %d bytes, %d segs\n\r", plen, n));
	net->virtio_net_tx(net, &iov[1], n - 1, plen);

	/* chain is processed, release it and set tlen */
	vq_relchain(vq, idx, tlen);
}

static void
virtio_net_ping_txq(void *vdev, struct virtio_vq_info *vq)
{
	struct virtio_net *net = vdev;

	/*
	 * Any ring entries to process?
	 */
	if (!vq_has_descs(vq))
		return;

	/* Signal the tx thread for processing */
	pthread_mutex_lock(&net->tx_mtx);
	vq->used->flags |= VRING_USED_F_NO_NOTIFY;
	if (net->tx_in_progress == 0)
		pthread_cond_signal(&net->tx_cond);
	pthread_mutex_unlock(&net->tx_mtx);
}

/*
 * Thread which will handle processing of TX desc
 */
static void *
virtio_net_tx_thread(void *param)
{
	struct virtio_net *net = param;
	struct virtio_vq_info *vq;
	int error;

	vq = &net->queues[VIRTIO_NET_TXQ];

	/*
	 * Let us wait till the tx queue pointers get initialised &
	 * first tx signaled
	 */
	pthread_mutex_lock(&net->tx_mtx);
	error = pthread_cond_wait(&net->tx_cond, &net->tx_mtx);
	assert(error == 0);
	if (net->closing) {
		WPRINTF(("vtnet tx thread closing...\n"));
		pthread_mutex_unlock(&net->tx_mtx);
		return NULL;
	}

	for (;;) {
		/* note - tx mutex is locked here */
		while (net->resetting || !vq_has_descs(vq)) {
			vq->used->flags &= ~VRING_USED_F_NO_NOTIFY;
			/* memory barrier */
			mb();
			if (!net->resetting && vq_has_descs(vq))
				break;

			net->tx_in_progress = 0;
			error = pthread_cond_wait(&net->tx_cond, &net->tx_mtx);
			assert(error == 0);
			if (net->closing) {
				WPRINTF(("vtnet tx thread closing...\n"));
				pthread_mutex_unlock(&net->tx_mtx);
				return NULL;
			}
		}
		vq->used->flags |= VRING_USED_F_NO_NOTIFY;
		net->tx_in_progress = 1;
		pthread_mutex_unlock(&net->tx_mtx);

		do {
			/*
			 * Run through entries, placing them into
			 * iovecs and sending when an end-of-packet
			 * is found
			 */
			virtio_net_proctx(net, vq);
		} while (vq_has_descs(vq));

		/*
		 * Generate an interrupt if needed.
		 */
		vq_endchains(vq, 1);

		pthread_mutex_lock(&net->tx_mtx);
	}
}

#ifdef notyet
static void
virtio_net_ping_ctlq(void *vdev, struct virtio_vq_info *vq)
{
	DPRINTF(("vtnet: control qnotify!\n\r"));
}
#endif

static int
virtio_net_parsemac(char *mac_str, uint8_t *mac_addr)
{
	struct ether_addr ether_addr;
	struct ether_addr *ea;
	char *tmpstr;
	char zero_addr[ETHER_ADDR_LEN] = { 0, 0, 0, 0, 0, 0 };

	tmpstr = strsep(&mac_str, "=");
	ea = &ether_addr;
	if ((mac_str != NULL) && (!strcmp(tmpstr, "mac"))) {
		ea = ether_aton(mac_str, ea);

		if (ea == NULL || ETHER_IS_MULTICAST(ea->ether_addr_octet) ||
		    memcmp(ea->ether_addr_octet, zero_addr, ETHER_ADDR_LEN)
				== 0) {
			fprintf(stderr, "Invalid MAC %s\n", mac_str);
			return -1;
		}
		memcpy(mac_addr, ea->ether_addr_octet, ETHER_ADDR_LEN);
	}

	return 0;
}

static int
virtio_net_tap_open(char *devname)
{
	int tunfd, rc;
	struct ifreq ifr;

#define PATH_NET_TUN "/dev/net/tun"
	tunfd = open(PATH_NET_TUN, O_RDWR);
	if (tunfd < 0) {
		WPRINTF(("open of tup device /dev/net/tun failed\n"));
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

	if (*devname)
		strncpy(ifr.ifr_name, devname, IFNAMSIZ);

	rc = ioctl(tunfd, TUNSETIFF, (void *)&ifr);
	if (rc < 0) {
		WPRINTF(("open of tap device %s failed\n", devname));
		close(tunfd);
		return -1;
	}

	strcpy(devname, ifr.ifr_name);
	return tunfd;
}

static void
virtio_net_tap_setup(struct virtio_net *net, char *devname)
{
	char tbuf[80 + 5];	/* room for "acrn_" prefix */
	char *tbuf_ptr;

	tbuf_ptr = tbuf;

	strcpy(tbuf, "acrn_");

	tbuf_ptr += 5;

	strncat(tbuf_ptr, devname, sizeof(tbuf) - 6);

	net->virtio_net_rx = virtio_net_tap_rx;
	net->virtio_net_tx = virtio_net_tap_tx;

	net->tapfd = virtio_net_tap_open(tbuf);
	if (net->tapfd == -1) {
		WPRINTF(("open of tap device %s failed\n", tbuf));
		return;
	}
	DPRINTF(("open of tap device %s success!\n", tbuf));

	/*
	 * Set non-blocking and register for read
	 * notifications with the event loop
	 */
	int opt = 1;

	if (ioctl(net->tapfd, FIONBIO, &opt) < 0) {
		WPRINTF(("tap device O_NONBLOCK failed\n"));
		close(net->tapfd);
		net->tapfd = -1;
	}

	net->mevp = mevent_add(net->tapfd, EVF_READ,
			       virtio_net_rx_callback, net);
	if (net->mevp == NULL) {
		WPRINTF(("Could not register event\n"));
		close(net->tapfd);
		net->tapfd = -1;
	}
}

static void
virtio_net_netmap_setup(struct virtio_net *net, char *ifname)
{
	net->virtio_net_rx = virtio_net_netmap_rx;
	net->virtio_net_tx = virtio_net_netmap_tx;

	net->nmd = nm_open(ifname, NULL, 0, 0);
	if (net->nmd == NULL) {
		WPRINTF(("open of netmap device %s failed\n", ifname));
		return;
	}

	net->mevp = mevent_add(net->nmd->fd, EVF_READ,
			       virtio_net_rx_callback, net);
	if (net->mevp == NULL) {
		WPRINTF(("Could not register event\n"));
		nm_close(net->nmd);
		net->nmd = NULL;
	}
}

static int
virtio_net_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	MD5_CTX mdctx;
	unsigned char digest[16];
	char nstr[80];
	char tname[MAXCOMLEN + 1];
	struct virtio_net *net;
	char *devname;
	char *vtopts;
	int mac_provided;
	pthread_mutexattr_t attr;
	int rc;

	net = calloc(1, sizeof(struct virtio_net));
	if (!net) {
		WPRINTF(("virtio_net: calloc returns NULL\n"));
		return -1;
	}

	/* init mutex attribute properly to avoid deadlock */
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		DPRINTF(("mutexattr init failed with erro %d!\n", rc));
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc)
		DPRINTF(("virtio_net: mutexattr_settype failed with "
			"error %d!\n", rc));

	rc = pthread_mutex_init(&net->mtx, &attr);
	if (rc)
		DPRINTF(("virtio_net: pthread_mutex_init failed with "
			"error %d!\n", rc));

	virtio_linkup(&net->base, &virtio_net_ops, net, dev, net->queues);
	net->base.mtx = &net->mtx;

	net->queues[VIRTIO_NET_RXQ].qsize = VIRTIO_NET_RINGSZ;
	net->queues[VIRTIO_NET_RXQ].notify = virtio_net_ping_rxq;
	net->queues[VIRTIO_NET_TXQ].qsize = VIRTIO_NET_RINGSZ;
	net->queues[VIRTIO_NET_TXQ].notify = virtio_net_ping_txq;
#ifdef notyet
	net->queues[VIRTIO_NET_CTLQ].qsize = VIRTIO_NET_RINGSZ;
	net->queues[VIRTIO_NET_CTLQ].notify = virtio_net_ping_ctlq;
#endif

	/*
	 * Attempt to open the tap device and read the MAC address
	 * if specified
	 */
	mac_provided = 0;
	net->tapfd = -1;
	net->nmd = NULL;
	if (opts != NULL) {
		int err;

		devname = vtopts = strdup(opts);
		if (!devname) {
			WPRINTF(("virtio_net: strdup returns NULL\n"));
			return -1;
		}

		(void) strsep(&vtopts, ",");

		if (vtopts != NULL) {
			err = virtio_net_parsemac(vtopts, net->config.mac);
			if (err != 0) {
				free(devname);
				return err;
			}
			mac_provided = 1;
		}

		if (strncmp(devname, "vale", 4) == 0)
			virtio_net_netmap_setup(net, devname);
		if (strncmp(devname, "tap", 3) == 0 ||
		    strncmp(devname, "vmnet", 5) == 0)
			virtio_net_tap_setup(net, devname);

		free(devname);
	}

	/*
	 * The default MAC address is the standard NetApp OUI of 00-a0-98,
	 * followed by an MD5 of the PCI slot/func number and dev name
	 */
	if (!mac_provided) {
		snprintf(nstr, sizeof(nstr), "%d-%d-%s", dev->slot,
		    dev->func, vmname);

		MD5_Init(&mdctx);
		MD5_Update(&mdctx, nstr, strlen(nstr));
		MD5_Final(digest, &mdctx);

		net->config.mac[0] = 0x00;
		net->config.mac[1] = 0x16;
		net->config.mac[2] = 0x3E;
		net->config.mac[3] = digest[0];
		net->config.mac[4] = digest[1];
		net->config.mac[5] = digest[2];
	}

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_NET);
	pci_set_cfgdata16(dev, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_NETWORK);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_NET);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	/* Link is up if we managed to open tap device or vale port. */
	net->config.status = (opts == NULL || net->tapfd >= 0 ||
			      net->nmd != NULL);

	/* use BAR 1 to map MSI-X table and PBA, if we're using MSI-X */
	if (virtio_interrupt_init(&net->base, virtio_uses_msix())) {
		if (net)
			free(net);
		return -1;
	}

	/* use BAR 0 to map config regs in IO space */
	virtio_set_io_bar(&net->base, 0);

	net->resetting = 0;
	net->closing = 0;

	net->rx_merge = 1;
	net->rx_vhdrlen = sizeof(struct virtio_net_rxhdr);
	net->rx_in_progress = 0;
	pthread_mutex_init(&net->rx_mtx, NULL);

	/*
	 * Initialize tx semaphore & spawn TX processing thread.
	 * As of now, only one thread for TX desc processing is
	 * spawned.
	 */
	net->tx_in_progress = 0;
	pthread_mutex_init(&net->tx_mtx, NULL);
	pthread_cond_init(&net->tx_cond, NULL);
	pthread_create(&net->tx_tid, NULL, virtio_net_tx_thread,
		       (void *)net);
	snprintf(tname, sizeof(tname), "vtnet-%d:%d tx", dev->slot,
		 dev->func);
	pthread_setname_np(net->tx_tid, tname);

	return 0;
}

static int
virtio_net_cfgwrite(void *vdev, int offset, int size, uint32_t value)
{
	struct virtio_net *net = vdev;
	void *ptr;

	if (offset < 6) {
		assert(offset + size <= 6);
		/*
		 * The driver is allowed to change the MAC address
		 */
		ptr = &net->config.mac[offset];
		memcpy(ptr, &value, size);
	} else {
		/* silently ignore other writes */
		DPRINTF(("vtnet: write to readonly reg %d\n\r", offset));
	}

	return 0;
}

static int
virtio_net_cfgread(void *vdev, int offset, int size, uint32_t *retval)
{
	struct virtio_net *net = vdev;
	void *ptr;

	ptr = (uint8_t *)&net->config + offset;
	memcpy(retval, ptr, size);
	return 0;
}

static void
virtio_net_neg_features(void *vdev, uint64_t negotiated_features)
{
	struct virtio_net *net = vdev;

	net->features = negotiated_features;

	if (!(net->features & VIRTIO_NET_F_MRG_RXBUF)) {
		net->rx_merge = 0;
		/* non-merge rx header is 2 bytes shorter */
		net->rx_vhdrlen -= 2;
	}
}

static void
virtio_net_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_net *net;

	if (dev->arg) {
		net = (struct virtio_net *) dev->arg;

		virtio_net_tx_stop(net);

		if (net->tapfd >= 0) {
			close(net->tapfd);
			net->tapfd = -1;
		} else
			fprintf(stderr, "net->tapfd is -1!\n");

		if (net->mevp != NULL)
			mevent_delete(net->mevp);

		free(net);

		DPRINTF(("%s: done\n", __func__));
	} else
		fprintf(stderr, "%s: NULL!\n", __func__);
}

struct pci_vdev_ops pci_ops_virtio_net = {
	.class_name	= "virtio-net",
	.vdev_init	= virtio_net_init,
	.vdev_deinit	= virtio_net_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_net);
