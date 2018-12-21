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

#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <openssl/md5.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"
#include "block_if.h"

#define VIRTIO_BLK_RINGSZ	64
#define VIRTIO_BLK_MAX_OPTS_LEN	256

#define VIRTIO_BLK_S_OK	0
#define VIRTIO_BLK_S_IOERR	1
#define	VIRTIO_BLK_S_UNSUPP	2

#define	VIRTIO_BLK_BLK_ID_BYTES	20

/* Capability bits */
#define	VIRTIO_BLK_F_SEG_MAX	(1 << 2)	/* Maximum request segments */
#define	VIRTIO_BLK_F_BLK_SIZE	(1 << 6)	/* cfg block size valid */
#define	VIRTIO_BLK_F_FLUSH	(1 << 9)	/* Cache flush support */
#define	VIRTIO_BLK_F_TOPOLOGY	(1 << 10)	/* Optimal I/O alignment */

/* Device can toggle its cache between writeback and writethrough modes */
#define	VIRTIO_BLK_F_CONFIG_WCE	(1 << 11)

#define	VIRTIO_BLK_F_DISCARD	(1 << 13)

/*
 * Basic device capabilities
 */
#define VIRTIO_BLK_S_HOSTCAPS      \
	(VIRTIO_BLK_F_SEG_MAX |						    \
	VIRTIO_BLK_F_BLK_SIZE |						    \
	VIRTIO_BLK_F_TOPOLOGY |						    \
	(1 << VIRTIO_RING_F_INDIRECT_DESC))	/* indirect descriptors */

/*
 * Writeback cache bits
 */
#define VIRTIO_BLK_F_WB_BITS	\
	(VIRTIO_BLK_F_FLUSH |	\
	VIRTIO_BLK_F_CONFIG_WCE)

/*
 * Config space "registers"
 */
struct virtio_blk_config {
	uint64_t capacity;
	uint32_t size_max;
	uint32_t seg_max;
	struct {
		uint16_t cylinders;
		uint8_t heads;
		uint8_t sectors;
	} geometry;
	uint32_t blk_size;
	struct {
		uint8_t physical_block_exp;
		uint8_t alignment_offset;
		uint16_t min_io_size;
		uint32_t opt_io_size;
	} topology;
	uint8_t	writeback;
	uint8_t unused;
	/* Reserve for num_queues when VIRTIO_BLK_F_MQ is support*/
	uint16_t reserve;
	/* The maximum discard sectors (in 512-byte sectors) for one segment */
	uint32_t max_discard_sectors;
	/* The maximum number of discard segments */
	uint32_t max_discard_seg;
	/* Discard commands must be aligned to this number of sectors. */
	uint32_t discard_sector_alignment;
} __attribute__((packed));

/*
 * Fixed-size block header
 */
struct virtio_blk_hdr {
#define	VBH_OP_READ		0
#define	VBH_OP_WRITE		1
#define	VBH_OP_FLUSH		4
#define	VBH_OP_FLUSH_OUT	5
#define	VBH_OP_IDENT		8
#define	VBH_OP_DISCARD		11
#define	VBH_FLAG_BARRIER	0x80000000	/* OR'ed into type */
	uint32_t type;
	uint32_t ioprio;
	uint64_t sector;
} __attribute__((packed));

/*
 * Debug printf
 */
static int virtio_blk_debug;
#define DPRINTF(params) do { if (virtio_blk_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

struct virtio_blk_ioreq {
	struct blockif_req req;
	struct virtio_blk *blk;
	uint8_t *status;
	uint16_t idx;
};

/*
 * Per-device struct
 */
struct virtio_blk {
	struct virtio_base base;
	pthread_mutex_t mtx;
	struct virtio_vq_info vq;
	struct virtio_blk_config cfg;
	struct blockif_ctxt *bc;
	char ident[VIRTIO_BLK_BLK_ID_BYTES + 1];
	struct virtio_blk_ioreq ios[VIRTIO_BLK_RINGSZ];
	uint8_t original_wce;
};

static void virtio_blk_reset(void *);
static void virtio_blk_notify(void *, struct virtio_vq_info *);
static int virtio_blk_cfgread(void *, int, int, uint32_t *);
static int virtio_blk_cfgwrite(void *, int, int, uint32_t);

static struct virtio_ops virtio_blk_ops = {
	"virtio_blk",		/* our name */
	1,			/* we support 1 virtqueue */
	sizeof(struct virtio_blk_config), /* config reg size */
	virtio_blk_reset,	/* reset */
	virtio_blk_notify,	/* device-wide qnotify */
	virtio_blk_cfgread,	/* read PCI config */
	virtio_blk_cfgwrite,	/* write PCI config */
	NULL,			/* apply negotiated features */
	NULL,			/* called on guest set status */
};

static void
virtio_blk_reset(void *vdev)
{
	struct virtio_blk *blk = vdev;

	DPRINTF(("virtio_blk: device reset requested !\n"));
	virtio_reset_dev(&blk->base);
	blockif_set_wce(blk->bc, blk->original_wce);
}

static void
virtio_blk_done(struct blockif_req *br, int err)
{
	struct virtio_blk_ioreq *io = br->param;
	struct virtio_blk *blk = io->blk;

	if (err)
		DPRINTF(("virtio_blk: done with error = %d\n\r", err));

	/* convert errno into a virtio block error return */
	if (err == EOPNOTSUPP || err == ENOSYS)
		*io->status = VIRTIO_BLK_S_UNSUPP;
	else if (err != 0)
		*io->status = VIRTIO_BLK_S_IOERR;
	else
		*io->status = VIRTIO_BLK_S_OK;

	/*
	 * Return the descriptor back to the host.
	 * We wrote 1 byte (our status) to host.
	 */
	pthread_mutex_lock(&blk->mtx);
	vq_relchain(&blk->vq, io->idx, 1);
	vq_endchains(&blk->vq, 0);
	pthread_mutex_unlock(&blk->mtx);
}

static void
virtio_blk_proc(struct virtio_blk *blk, struct virtio_vq_info *vq)
{
	struct virtio_blk_hdr *vbh;
	struct virtio_blk_ioreq *io;
	int i, n;
	int err;
	ssize_t iolen;
	int writeop, type;
	struct iovec iov[BLOCKIF_IOV_MAX + 2];
	uint16_t idx, flags[BLOCKIF_IOV_MAX + 2];

	n = vq_getchain(vq, &idx, iov, BLOCKIF_IOV_MAX + 2, flags);

	/*
	 * The first descriptor will be the read-only fixed header,
	 * and the last is for status (hence +2 above and below).
	 * The remaining iov's are the actual data I/O vectors.
	 *
	 * XXX - note - this fails on crash dump, which does a
	 * VIRTIO_BLK_T_FLUSH with a zero transfer length
	 */
	assert(n >= 2 && n <= BLOCKIF_IOV_MAX + 2);

	io = &blk->ios[idx];
	assert((flags[0] & VRING_DESC_F_WRITE) == 0);
	assert(iov[0].iov_len == sizeof(struct virtio_blk_hdr));
	vbh = iov[0].iov_base;
	memcpy(&io->req.iov, &iov[1], sizeof(struct iovec) * (n - 2));
	io->req.iovcnt = n - 2;
	io->req.offset = vbh->sector * DEV_BSIZE;
	io->status = iov[--n].iov_base;
	assert(iov[n].iov_len == 1);
	assert(flags[n] & VRING_DESC_F_WRITE);

	/*
	 * XXX
	 * The guest should not be setting the BARRIER flag because
	 * we don't advertise the capability.
	 */
	type = vbh->type & ~VBH_FLAG_BARRIER;
	writeop = ((type == VBH_OP_WRITE) ||
			(type == VBH_OP_DISCARD));

	iolen = 0;
	for (i = 1; i < n; i++) {
		/*
		 * - write/discard op implies read-only descriptor,
		 * - read/ident op implies write-only descriptor,
		 * therefore test the inverse of the descriptor bit
		 * to the op.
		 */
		assert(((flags[i] & VRING_DESC_F_WRITE) == 0) == writeop);
		iolen += iov[i].iov_len;
	}
	io->req.resid = iolen;

	DPRINTF(("virtio_blk: %s op, %zd bytes, %d segs, offset %ld\n\r",
		 writeop ? "write/discard" : "read/ident", iolen, i - 1,
		 io->req.offset));

	switch (type) {
	case VBH_OP_READ:
	case VBH_OP_WRITE:
		/*
		 * VirtIO v1.0 spec 04 5.2.5:
		 * - Protocol unit size is always 512 bytes.
		 * - blk_size (logical block size) and physical_block_exp
		 *   (physical block size) do not affect the units in the
		 *   protocol, only performance.
		 *
		 * VirtIO v1.0 spec 04 5.2.6.1:
		 * - A driver MUST NOT submit a request which would cause a
		 *   read or write beyond capacity.
		 */
		if ((iolen & (DEV_BSIZE - 1)) ||
		    vbh->sector + iolen / DEV_BSIZE > blk->cfg.capacity) {
			DPRINTF(("virtio_blk: invalid request, iolen = %ld, "
			         "sector = %lu, capacity = %lu\n\r", iolen,
			         vbh->sector, blk->cfg.capacity));
			virtio_blk_done(&io->req, EINVAL);
			return;
		}

		err = ((type == VBH_OP_READ) ? blockif_read : blockif_write)
				(blk->bc, &io->req);
		break;
	case VBH_OP_DISCARD:
		err = blockif_discard(blk->bc, &io->req);
		break;
	case VBH_OP_FLUSH:
	case VBH_OP_FLUSH_OUT:
		err = blockif_flush(blk->bc, &io->req);
		break;
	case VBH_OP_IDENT:
		/* Assume a single buffer */
		/* S/n equal to buffer is not zero-terminated. */
		memset(iov[1].iov_base, 0, iov[1].iov_len);
		strncpy(iov[1].iov_base, blk->ident,
		    MIN(iov[1].iov_len, sizeof(blk->ident)));
		virtio_blk_done(&io->req, 0);
		return;
	default:
		virtio_blk_done(&io->req, EOPNOTSUPP);
		return;
	}
	assert(err == 0);
}

static void
virtio_blk_notify(void *vdev, struct virtio_vq_info *vq)
{
	struct virtio_blk *blk = vdev;

	while (vq_has_descs(vq))
		virtio_blk_proc(blk, vq);
}

static uint64_t
virtio_blk_get_caps(struct virtio_blk *blk, bool wb)
{
	uint64_t caps;

	caps = VIRTIO_BLK_S_HOSTCAPS;
	if (wb)
		caps |= VIRTIO_BLK_F_WB_BITS;

	if (blockif_candiscard(blk->bc))
		caps |= VIRTIO_BLK_F_DISCARD;

	return caps;
}

static int
virtio_blk_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	char bident[16];
	struct blockif_ctxt *bctxt;
	MD5_CTX mdctx;
	u_char digest[16];
	struct virtio_blk *blk;
	off_t size;
	int i, sectsz, sts, sto;
	pthread_mutexattr_t attr;
	int rc;

	if (opts == NULL) {
		printf("virtio_blk: backing device required\n");
		return -1;
	}

	/*
	 * The supplied backing file has to exist
	 */
	if (snprintf(bident, sizeof(bident), "%d:%d",
				dev->slot, dev->func) >= sizeof(bident)) {
		WPRINTF(("bident error, please check slot and func\n"));
	}
	bctxt = blockif_open(opts, bident);
	if (bctxt == NULL) {
		perror("Could not open backing file");
		return -1;
	}

	size = blockif_size(bctxt);
	sectsz = blockif_sectsz(bctxt);
	blockif_psectsz(bctxt, &sts, &sto);

	blk = calloc(1, sizeof(struct virtio_blk));
	if (!blk) {
		WPRINTF(("virtio_blk: calloc returns NULL\n"));
		return -1;
	}

	blk->bc = bctxt;
	for (i = 0; i < VIRTIO_BLK_RINGSZ; i++) {
		struct virtio_blk_ioreq *io = &blk->ios[i];

		io->req.callback = virtio_blk_done;
		io->req.param = io;
		io->blk = blk;
		io->idx = i;
	}

	/* init mutex attribute properly to avoid deadlock */
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		DPRINTF(("mutexattr init failed with erro %d!\n", rc));
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc)
		DPRINTF(("virtio_blk: mutexattr_settype failed with "
					"error %d!\n", rc));

	rc = pthread_mutex_init(&blk->mtx, &attr);
	if (rc)
		DPRINTF(("virtio_blk: pthread_mutex_init failed with "
					"error %d!\n", rc));

	/* init virtio struct and virtqueues */
	virtio_linkup(&blk->base, &virtio_blk_ops, blk, dev, &blk->vq, BACKEND_VBSU);
	blk->base.mtx = &blk->mtx;

	blk->vq.qsize = VIRTIO_BLK_RINGSZ;
	/* blk->vq.vq_notify = we have no per-queue notify */

	/*
	 * Create an identifier for the backing file. Use parts of the
	 * md5 sum of the filename
	 */
	MD5_Init(&mdctx);
	MD5_Update(&mdctx, opts, strnlen(opts, VIRTIO_BLK_MAX_OPTS_LEN));
	MD5_Final(digest, &mdctx);
	if (snprintf(blk->ident, sizeof(blk->ident),
		"ACRN--%02X%02X-%02X%02X-%02X%02X", digest[0],
		digest[1], digest[2], digest[3], digest[4],
		digest[5]) >= sizeof(blk->ident)) {
		WPRINTF(("virtio_blk: block ident too long\n"));
	}

	/* setup virtio block config space */
	blk->cfg.capacity = size / DEV_BSIZE; /* 512-byte units */
	blk->cfg.size_max = 0;	/* not negotiated */
	blk->cfg.seg_max = BLOCKIF_IOV_MAX;
	blk->cfg.geometry.cylinders = 0;	/* no geometry */
	blk->cfg.geometry.heads = 0;
	blk->cfg.geometry.sectors = 0;
	blk->cfg.blk_size = sectsz;
	blk->cfg.topology.physical_block_exp =
	    (sts > sectsz) ? (ffsll(sts / sectsz) - 1) : 0;
	blk->cfg.topology.alignment_offset =
	    (sto != 0) ? ((sts - sto) / sectsz) : 0;
	blk->cfg.topology.min_io_size = 0;
	blk->cfg.topology.opt_io_size = 0;
	blk->cfg.writeback = blockif_get_wce(blk->bc);
	blk->original_wce = blk->cfg.writeback; /* save for reset */
	if (blockif_candiscard(blk->bc)) {
		blk->cfg.max_discard_sectors = blockif_max_discard_sectors(blk->bc);
		blk->cfg.max_discard_seg = blockif_max_discard_seg(blk->bc);
		blk->cfg.discard_sector_alignment = blockif_discard_sector_alignment(blk->bc);
	}
	blk->base.device_caps =
		virtio_blk_get_caps(blk, !!blk->cfg.writeback);

	/*
	 * Should we move some of this into virtio.c?  Could
	 * have the device, class, and subdev_0 as fields in
	 * the virtio constants structure.
	 */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_BLOCK);
	pci_set_cfgdata16(dev, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_STORAGE);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_BLOCK);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	if (virtio_interrupt_init(&blk->base, virtio_uses_msix())) {
		blockif_close(blk->bc);
		free(blk);
		return -1;
	}
	virtio_set_io_bar(&blk->base, 0);
	return 0;
}

static void
virtio_blk_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct blockif_ctxt *bctxt;
	struct virtio_blk *blk;

	if (dev->arg) {
		DPRINTF(("virtio_blk: deinit\n"));
		blk = (struct virtio_blk *) dev->arg;
		bctxt = blk->bc;
		if (blockif_flush_all(bctxt))
			WPRINTF(("vrito_blk:"
				"Failed to flush before close\n"));
		blockif_close(bctxt);
		free(blk);
	}
}

static int
virtio_blk_cfgwrite(void *vdev, int offset, int size, uint32_t value)
{
	struct virtio_blk *blk = vdev;
	struct virtio_blk_config *blkcfg = &(blk->cfg);
	void *ptr;

	ptr = (uint8_t *)blkcfg + offset;

	if ((offset == offsetof(struct virtio_blk_config, writeback))
		&& (size == 1)) {
		memcpy(ptr, &value, size);
		blockif_set_wce(blk->bc, blkcfg->writeback);
		if (blkcfg->writeback)
			blk->base.device_caps |= VIRTIO_BLK_F_FLUSH;
		else
			blk->base.device_caps &= ~VIRTIO_BLK_F_FLUSH;
		return 0;
	}

	DPRINTF(("virtio_blk: write to readonly reg %d\n\r", offset));
	return -1;
}

static int
virtio_blk_cfgread(void *vdev, int offset, int size, uint32_t *retval)
{
	struct virtio_blk *blk = vdev;
	void *ptr;

	/* our caller has already verified offset and size */
	ptr = (uint8_t *)&blk->cfg + offset;
	memcpy(retval, ptr, size);
	return 0;
}

struct pci_vdev_ops pci_ops_virtio_blk = {
	.class_name	= "virtio-blk",
	.vdev_init	= virtio_blk_init,
	.vdev_deinit	= virtio_blk_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_blk);
