/*-
 * Copyright (c) 2013  Chris Torek <torek @ torek net>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"

/*
 * Functions for dealing with generalized "virtual devices" as
 * defined by <https://www.google.com/#output=search&q=virtio+spec>
 */

/*
 * In case we decide to relax the "virtio struct comes at the
 * front of virtio-based device struct" constraint, let's use
 * this to convert.
 */
#define DEV_STRUCT(vs) ((void *)(vs))

/*
 * Link a virtio_base to its constants, the virtio device, and
 * the PCI emulation.
 */
void
virtio_linkup(struct virtio_base *base, struct virtio_ops *vops,
	      void *pci_virtio_dev, struct pci_vdev *dev,
	      struct virtio_vq_info *queues)
{
	int i;

	/* base and pci_virtio_dev addresses must match */
	assert((void *)base == pci_virtio_dev);
	base->vops = vops;
	base->dev = dev;
	dev->arg = base;

	base->queues = queues;
	for (i = 0; i < vops->nvq; i++) {
		queues[i].base = base;
		queues[i].num = i;
	}
}

/*
 * Reset device (device-wide).  This erases all queues, i.e.,
 * all the queues become invalid (though we don't wipe out the
 * internal pointers, we just clear the VQ_ALLOC flag).
 *
 * It resets negotiated features to "none".
 *
 * If MSI-X is enabled, this also resets all the vectors to NO_VECTOR.
 */
void
virtio_reset_dev(struct virtio_base *base)
{
	struct virtio_vq_info *vq;
	int i, nvq;

/* if (base->mtx) */
/* assert(pthread_mutex_isowned_np(base->mtx)); */

	nvq = base->vops->nvq;
	for (vq = base->queues, i = 0; i < nvq; vq++, i++) {
		vq->flags = 0;
		vq->last_avail = 0;
		vq->save_used = 0;
		vq->pfn = 0;
		vq->msix_idx = VIRTIO_MSI_NO_VECTOR;
		vq->gpa_desc[0] = 0;
		vq->gpa_desc[1] = 0;
		vq->gpa_avail[0] = 0;
		vq->gpa_avail[1] = 0;
		vq->gpa_used[0] = 0;
		vq->gpa_used[1] = 0;
		vq->enabled = 0;
	}
	base->negotiated_caps = 0;
	base->curq = 0;
	/* base->status = 0; -- redundant */
	if (base->isr)
		pci_lintr_deassert(base->dev);
	base->isr = 0;
	base->msix_cfg_idx = VIRTIO_MSI_NO_VECTOR;
	base->device_feature_select = 0;
	base->driver_feature_select = 0;
	base->config_generation = 0;
}

/*
 * Set I/O BAR (usually 0) to map PCI config registers.
 */
void
virtio_set_io_bar(struct virtio_base *base, int barnum)
{
	size_t size;

	/*
	 * ??? should we use CFG0 if MSI-X is disabled?
	 * Existing code did not...
	 */
	size = VIRTIO_CR_CFG1 + base->vops->cfgsize;
	pci_emul_alloc_bar(base->dev, barnum, PCIBAR_IO, size);
}

/*
 * Initialize MSI-X vector capabilities if we're to use MSI-X,
 * or MSI capabilities if not.
 *
 * We assume we want one MSI-X vector per queue, here, plus one
 * for the config vec.
 */
int
virtio_intr_init(struct virtio_base *base, int barnum, int use_msix)
{
	int nvec;

	if (use_msix) {
		base->flags |= VIRTIO_USE_MSIX;
		VIRTIO_BASE_LOCK(base);
		virtio_reset_dev(base); /* set all vectors to NO_VECTOR */
		VIRTIO_BASE_UNLOCK(base);
		nvec = base->vops->nvq + 1;
		if (pci_emul_add_msixcap(base->dev, nvec, barnum))
			return -1;
	} else
		base->flags &= ~VIRTIO_USE_MSIX;

	/* Only 1 MSI vector for acrn-dm */
	pci_emul_add_msicap(base->dev, 1);

	/* Legacy interrupts are mandatory for virtio devices */
	pci_lintr_request(base->dev);

	return 0;
}

/*
 * Initialize MSI-X vector capabilities if we're to use MSI-X,
 * or MSI capabilities if not.
 *
 * Wrapper function for virtio_intr_init() since by default we
 * will use bar 1 for MSI-X.
 */
int
virtio_interrupt_init(struct virtio_base *base, int use_msix)
{
	return virtio_intr_init(base, 1, use_msix);
}

/*
 * Initialize the currently-selected virtio queue (base->curq).
 * The guest just gave us a page frame number, from which we can
 * calculate the addresses of the queue.
 */
void
virtio_vq_init(struct virtio_base *base, uint32_t pfn)
{
	struct virtio_vq_info *vq;
	uint64_t phys;
	size_t size;
	char *vb;

	vq = &base->queues[base->curq];
	vq->pfn = pfn;
	phys = (uint64_t)pfn << VRING_PAGE_BITS;
	size = vring_size(vq->qsize);
	vb = paddr_guest2host(base->dev->vmctx, phys, size);

	/* First page(s) are descriptors... */
	vq->desc = (struct virtio_desc *)vb;
	vb += vq->qsize * sizeof(struct virtio_desc);

	/* ... immediately followed by "avail" ring (entirely uint16_t's) */
	vq->avail = (struct vring_avail *)vb;
	vb += (2 + vq->qsize + 1) * sizeof(uint16_t);

	/* Then it's rounded up to the next page... */
	vb = (char *)roundup2((uintptr_t)vb, VRING_ALIGN);

	/* ... and the last page(s) are the used ring. */
	vq->used = (struct vring_used *)vb;

	/* Mark queue as allocated, and start at 0 when we use it. */
	vq->flags = VQ_ALLOC;
	vq->last_avail = 0;
	vq->save_used = 0;
}

/*
 * Helper inline for vq_getchain(): record the i'th "real"
 * descriptor.
 */
static inline void
_vq_record(int i, volatile struct virtio_desc *vd, struct vmctx *ctx,
	   struct iovec *iov, int n_iov, uint16_t *flags) {

	if (i >= n_iov)
		return;
	iov[i].iov_base = paddr_guest2host(ctx, vd->addr, vd->len);
	iov[i].iov_len = vd->len;
	if (flags != NULL)
		flags[i] = vd->flags;
}
#define	VQ_MAX_DESCRIPTORS	512	/* see below */

/*
 * Examine the chain of descriptors starting at the "next one" to
 * make sure that they describe a sensible request.  If so, return
 * the number of "real" descriptors that would be needed/used in
 * acting on this request.  This may be smaller than the number of
 * available descriptors, e.g., if there are two available but
 * they are two separate requests, this just returns 1.  Or, it
 * may be larger: if there are indirect descriptors involved,
 * there may only be one descriptor available but it may be an
 * indirect pointing to eight more.  We return 8 in this case,
 * i.e., we do not count the indirect descriptors, only the "real"
 * ones.
 *
 * Basically, this vets the flags and vd_next field of each
 * descriptor and tells you how many are involved.  Since some may
 * be indirect, this also needs the vmctx (in the pci_vdev
 * at base->dev) so that it can find indirect descriptors.
 *
 * As we process each descriptor, we copy and adjust it (guest to
 * host address wise, also using the vmtctx) into the given iov[]
 * array (of the given size).  If the array overflows, we stop
 * placing values into the array but keep processing descriptors,
 * up to VQ_MAX_DESCRIPTORS, before giving up and returning -1.
 * So you, the caller, must not assume that iov[] is as big as the
 * return value (you can process the same thing twice to allocate
 * a larger iov array if needed, or supply a zero length to find
 * out how much space is needed).
 *
 * If you want to verify the WRITE flag on each descriptor, pass a
 * non-NULL "flags" pointer to an array of "uint16_t" of the same size
 * as n_iov and we'll copy each flags field after unwinding any
 * indirects.
 *
 * If some descriptor(s) are invalid, this prints a diagnostic message
 * and returns -1.  If no descriptors are ready now it simply returns 0.
 *
 * You are assumed to have done a vq_ring_ready() if needed (note
 * that vq_has_descs() does one).
 */
int
vq_getchain(struct virtio_vq_info *vq, uint16_t *pidx,
	    struct iovec *iov, int n_iov, uint16_t *flags)
{
	int i;
	u_int ndesc, n_indir;
	u_int idx, next;

	volatile struct virtio_desc *vdir, *vindir, *vp;
	struct vmctx *ctx;
	struct virtio_base *base;
	const char *name;

	base = vq->base;
	name = base->vops->name;

	/*
	 * Note: it's the responsibility of the guest not to
	 * update vq->avail->idx until all of the descriptors
	 * the guest has written are valid (including all their
	 * next fields and vd_flags).
	 *
	 * Compute (last_avail - idx) in integers mod 2**16.  This is
	 * the number of descriptors the device has made available
	 * since the last time we updated vq->last_avail.
	 *
	 * We just need to do the subtraction as an unsigned int,
	 * then trim off excess bits.
	 */
	idx = vq->last_avail;
	ndesc = (uint16_t)((u_int)vq->avail->idx - idx);
	if (ndesc == 0)
		return 0;
	if (ndesc > vq->qsize) {
		/* XXX need better way to diagnose issues */
		fprintf(stderr,
		    "%s: ndesc (%u) out of range, driver confused?\r\n",
		    name, (u_int)ndesc);
		return -1;
	}

	/*
	 * Now count/parse "involved" descriptors starting from
	 * the head of the chain.
	 *
	 * To prevent loops, we could be more complicated and
	 * check whether we're re-visiting a previously visited
	 * index, but we just abort if the count gets excessive.
	 */
	ctx = base->dev->vmctx;
	*pidx = next = vq->avail->ring[idx & (vq->qsize - 1)];
	vq->last_avail++;
	for (i = 0; i < VQ_MAX_DESCRIPTORS; next = vdir->next) {
		if (next >= vq->qsize) {
			fprintf(stderr,
			    "%s: descriptor index %u out of range, "
			    "driver confused?\r\n",
			    name, next);
			return -1;
		}
		vdir = &vq->desc[next];
		if ((vdir->flags & VRING_DESC_F_INDIRECT) == 0) {
			_vq_record(i, vdir, ctx, iov, n_iov, flags);
			i++;
		} else if ((base->vops->hv_caps &
		    VIRTIO_RING_F_INDIRECT_DESC) == 0) {
			fprintf(stderr,
			    "%s: descriptor has forbidden INDIRECT flag, "
			    "driver confused?\r\n",
			    name);
			return -1;
		} else {
			n_indir = vdir->len / 16;
			if ((vdir->len & 0xf) || n_indir == 0) {
				fprintf(stderr,
				    "%s: invalid indir len 0x%x, "
				    "driver confused?\r\n",
				    name, (u_int)vdir->len);
				return -1;
			}
			vindir = paddr_guest2host(ctx,
			    vdir->addr, vdir->len);
			/*
			 * Indirects start at the 0th, then follow
			 * their own embedded "next"s until those run
			 * out.  Each one's indirect flag must be off
			 * (we don't really have to check, could just
			 * ignore errors...).
			 */
			next = 0;
			for (;;) {
				vp = &vindir[next];
				if (vp->flags & VRING_DESC_F_INDIRECT) {
					fprintf(stderr,
					    "%s: indirect desc has INDIR flag,"
					    " driver confused?\r\n",
					    name);
					return -1;
				}
				_vq_record(i, vp, ctx, iov, n_iov, flags);
				if (++i > VQ_MAX_DESCRIPTORS)
					goto loopy;
				if ((vp->flags & VRING_DESC_F_NEXT) == 0)
					break;
				next = vp->next;
				if (next >= n_indir) {
					fprintf(stderr,
					    "%s: invalid next %u > %u, "
					    "driver confused?\r\n",
					    name, (u_int)next, n_indir);
					return -1;
				}
			}
		}
		if ((vdir->flags & VRING_DESC_F_NEXT) == 0)
			return i;
	}
loopy:
	fprintf(stderr,
	    "%s: descriptor loop? count > %d - driver confused?\r\n",
	    name, i);
	return -1;
}

/*
 * Return the currently-first request chain back to the available queue.
 *
 * (This chain is the one you handled when you called vq_getchain()
 * and used its positive return value.)
 */
void
vq_retchain(struct virtio_vq_info *vq)
{
	vq->last_avail--;
}

/*
 * Return specified request chain to the guest, setting its I/O length
 * to the provided value.
 *
 * (This chain is the one you handled when you called vq_getchain()
 * and used its positive return value.)
 */
void
vq_relchain(struct virtio_vq_info *vq, uint16_t idx, uint32_t iolen)
{
	uint16_t uidx, mask;
	volatile struct vring_used *vuh;
	volatile struct virtio_used *vue;

	/*
	 * Notes:
	 *  - mask is N-1 where N is a power of 2 so computes x % N
	 *  - vuh points to the "used" data shared with guest
	 *  - vue points to the "used" ring entry we want to update
	 *  - head is the same value we compute in vq_iovecs().
	 *
	 * (I apologize for the two fields named idx; the
	 * virtio spec calls the one that vue points to, "id"...)
	 */
	mask = vq->qsize - 1;
	vuh = vq->used;

	uidx = vuh->idx;
	vue = &vuh->ring[uidx++ & mask];
	vue->idx = idx;
	vue->tlen = iolen;
	vuh->idx = uidx;
}

/*
 * Driver has finished processing "available" chains and calling
 * vq_relchain on each one.  If driver used all the available
 * chains, used_all should be set.
 *
 * If the "used" index moved we may need to inform the guest, i.e.,
 * deliver an interrupt.  Even if the used index did NOT move we
 * may need to deliver an interrupt, if the avail ring is empty and
 * we are supposed to interrupt on empty.
 *
 * Note that used_all_avail is provided by the caller because it's
 * a snapshot of the ring state when he decided to finish interrupt
 * processing -- it's possible that descriptors became available after
 * that point.  (It's also typically a constant 1/True as well.)
 */
void
vq_endchains(struct virtio_vq_info *vq, int used_all_avail)
{
	struct virtio_base *base;
	uint16_t event_idx, new_idx, old_idx;
	int intr;

	/*
	 * Interrupt generation: if we're using EVENT_IDX,
	 * interrupt if we've crossed the event threshold.
	 * Otherwise interrupt is generated if we added "used" entries,
	 * but suppressed by VRING_AVAIL_F_NO_INTERRUPT.
	 *
	 * In any case, though, if NOTIFY_ON_EMPTY is set and the
	 * entire avail was processed, we need to interrupt always.
	 */
	base = vq->base;
	old_idx = vq->save_used;
	vq->save_used = new_idx = vq->used->idx;
	if (used_all_avail &&
	    (base->negotiated_caps & VIRTIO_F_NOTIFY_ON_EMPTY))
		intr = 1;
	else if (base->negotiated_caps & VIRTIO_RING_F_EVENT_IDX) {
		event_idx = VQ_USED_EVENT_IDX(vq);
		/*
		 * This calculation is per docs and the kernel
		 * (see src/sys/dev/virtio/virtio_ring.h).
		 */
		intr = (uint16_t)(new_idx - event_idx - 1) <
			(uint16_t)(new_idx - old_idx);
	} else {
		intr = new_idx != old_idx &&
		    !(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT);
	}
	if (intr)
		vq_interrupt(base, vq);
}

/* Note: these are in sorted order to make for a fast search */
static struct config_reg {
	uint16_t	offset;	/* register offset */
	uint8_t		size;	/* size (bytes) */
	uint8_t		ro;		/* true => reg is read only */
	const char	*name;	/* name of reg */
} config_regs[] = {
	{ VIRTIO_CR_HOSTCAP,	4, 1, "HOSTCAP" },
	{ VIRTIO_CR_GUESTCAP,	4, 0, "GUESTCAP" },
	{ VIRTIO_CR_PFN,	4, 0, "PFN" },
	{ VIRTIO_CR_QNUM,	2, 1, "QNUM" },
	{ VIRTIO_CR_QSEL,	2, 0, "QSEL" },
	{ VIRTIO_CR_QNOTIFY,	2, 0, "QNOTIFY" },
	{ VIRTIO_CR_STATUS,	1, 0, "STATUS" },
	{ VIRTIO_CR_ISR,	1, 0, "ISR" },
	{ VIRTIO_CR_CFGVEC,	2, 0, "CFGVEC" },
	{ VIRTIO_CR_QVEC,	2, 0, "QVEC" },
};

static inline struct config_reg *
virtio_find_cr(int offset) {
	u_int hi, lo, mid;
	struct config_reg *cr;

	lo = 0;
	hi = sizeof(config_regs) / sizeof(*config_regs) - 1;
	while (hi >= lo) {
		mid = (hi + lo) >> 1;
		cr = &config_regs[mid];
		if (cr->offset == offset)
			return cr;
		if (cr->offset < offset)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

/*
 * Handle pci config space reads.
 * If it's to the MSI-X info, do that.
 * If it's part of the virtio standard stuff, do that.
 * Otherwise dispatch to the actual driver.
 */
uint64_t
virtio_pci_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		int baridx, uint64_t offset, int size)
{
	struct virtio_base *base = dev->arg;
	struct virtio_ops *vops;
	struct config_reg *cr;
	uint64_t virtio_config_size, max;
	const char *name;
	uint32_t newoff;
	uint32_t value;
	int error;

	if (base->flags & VIRTIO_USE_MSIX) {
		if (baridx == pci_msix_table_bar(dev) ||
		    baridx == pci_msix_pba_bar(dev)) {
			return pci_emul_msix_tread(dev, offset, size);
		}
	}

	/* XXX probably should do something better than just assert() */
	assert(baridx == 0);

	if (base->mtx)
		pthread_mutex_lock(base->mtx);

	vops = base->vops;
	name = vops->name;
	value = size == 1 ? 0xff : size == 2 ? 0xffff : 0xffffffff;

	if (size != 1 && size != 2 && size != 4)
		goto bad;

	if (pci_msix_enabled(dev))
		virtio_config_size = VIRTIO_CR_CFG1;
	else
		virtio_config_size = VIRTIO_CR_CFG0;

	if (offset >= virtio_config_size) {
		/*
		 * Subtract off the standard size (including MSI-X
		 * registers if enabled) and dispatch to underlying driver.
		 * If that fails, fall into general code.
		 */
		newoff = offset - virtio_config_size;
		max = vops->cfgsize ? vops->cfgsize : 0x100000000;
		if (newoff + size > max)
			goto bad;
		error = (*vops->cfgread)(DEV_STRUCT(base), newoff,
					 size, &value);
		if (!error)
			goto done;
	}

bad:
	cr = virtio_find_cr(offset);
	if (cr == NULL || cr->size != size) {
		if (cr != NULL) {
			/* offset must be OK, so size must be bad */
			fprintf(stderr,
			    "%s: read from %s: bad size %d\r\n",
			    name, cr->name, size);
		} else {
			fprintf(stderr,
			    "%s: read from bad offset/size %jd/%d\r\n",
			    name, (uintmax_t)offset, size);
		}
		goto done;
	}

	switch (offset) {
	case VIRTIO_CR_HOSTCAP:
		value = vops->hv_caps;
		break;
	case VIRTIO_CR_GUESTCAP:
		value = base->negotiated_caps;
		break;
	case VIRTIO_CR_PFN:
		if (base->curq < vops->nvq)
			value = base->queues[base->curq].pfn;
		break;
	case VIRTIO_CR_QNUM:
		value = base->curq < vops->nvq ?
			base->queues[base->curq].qsize : 0;
		break;
	case VIRTIO_CR_QSEL:
		value = base->curq;
		break;
	case VIRTIO_CR_QNOTIFY:
		value = 0;	/* XXX */
		break;
	case VIRTIO_CR_STATUS:
		value = base->status;
		break;
	case VIRTIO_CR_ISR:
		value = base->isr;
		base->isr = 0;		/* a read clears this flag */
		if (value)
			pci_lintr_deassert(dev);
		break;
	case VIRTIO_CR_CFGVEC:
		value = base->msix_cfg_idx;
		break;
	case VIRTIO_CR_QVEC:
		value = base->curq < vops->nvq ?
		    base->queues[base->curq].msix_idx :
		    VIRTIO_MSI_NO_VECTOR;
		break;
	}
done:
	if (base->mtx)
		pthread_mutex_unlock(base->mtx);
	return value;
}

/*
 * Handle pci config space writes.
 * If it's to the MSI-X info, do that.
 * If it's part of the virtio standard stuff, do that.
 * Otherwise dispatch to the actual driver.
 */
void
virtio_pci_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		 int baridx, uint64_t offset, int size, uint64_t value)
{
	struct virtio_base *base = dev->arg;
	struct virtio_vq_info *vq;
	struct virtio_ops *vops;
	struct config_reg *cr;
	uint64_t virtio_config_size, max;
	const char *name;
	uint32_t newoff;
	int error;

	if (base->flags & VIRTIO_USE_MSIX) {
		if (baridx == pci_msix_table_bar(dev) ||
		    baridx == pci_msix_pba_bar(dev)) {
			pci_emul_msix_twrite(dev, offset, size, value);
			return;
		}
	}

	/* XXX probably should do something better than just assert() */
	assert(baridx == 0);

	if (base->mtx)
		pthread_mutex_lock(base->mtx);

	vops = base->vops;
	name = vops->name;

	if (size != 1 && size != 2 && size != 4)
		goto bad;

	if (pci_msix_enabled(dev))
		virtio_config_size = VIRTIO_CR_CFG1;
	else
		virtio_config_size = VIRTIO_CR_CFG0;

	if (offset >= virtio_config_size) {
		/*
		 * Subtract off the standard size (including MSI-X
		 * registers if enabled) and dispatch to underlying driver.
		 */
		newoff = offset - virtio_config_size;
		max = vops->cfgsize ? vops->cfgsize : 0x100000000;
		if (newoff + size > max)
			goto bad;
		error = (*vops->cfgwrite)(DEV_STRUCT(base), newoff,
					  size, value);
		if (!error)
			goto done;
	}

bad:
	cr = virtio_find_cr(offset);
	if (cr == NULL || cr->size != size || cr->ro) {
		if (cr != NULL) {
			/* offset must be OK, wrong size and/or reg is R/O */
			if (cr->size != size)
				fprintf(stderr,
				    "%s: write to %s: bad size %d\r\n",
				    name, cr->name, size);
			if (cr->ro)
				fprintf(stderr,
				    "%s: write to read-only reg %s\r\n",
				    name, cr->name);
		} else {
			fprintf(stderr,
			    "%s: write to bad offset/size %jd/%d\r\n",
			    name, (uintmax_t)offset, size);
		}
		goto done;
	}

	switch (offset) {
	case VIRTIO_CR_GUESTCAP:
		base->negotiated_caps = value & vops->hv_caps;
		if (vops->apply_features)
			(*vops->apply_features)(DEV_STRUCT(base),
			    base->negotiated_caps);
		break;
	case VIRTIO_CR_PFN:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		virtio_vq_init(base, value);
		break;
	case VIRTIO_CR_QSEL:
		/*
		 * Note that the guest is allowed to select an
		 * invalid queue; we just need to return a QNUM
		 * of 0 while the bad queue is selected.
		 */
		base->curq = value;
		break;
	case VIRTIO_CR_QNOTIFY:
		if (value >= vops->nvq) {
			fprintf(stderr, "%s: queue %d notify out of range\r\n",
				name, (int)value);
			goto done;
		}
		vq = &base->queues[value];
		if (vq->notify)
			(*vq->notify)(DEV_STRUCT(base), vq);
		else if (vops->qnotify)
			(*vops->qnotify)(DEV_STRUCT(base), vq);
		else
			fprintf(stderr,
			    "%s: qnotify queue %d: missing vq/vops notify\r\n",
				name, (int)value);
		break;
	case VIRTIO_CR_STATUS:
		base->status = value;
		if (vops->set_status)
			(*vops->set_status)(DEV_STRUCT(base), value);
		if (value == 0)
			(*vops->reset)(DEV_STRUCT(base));
		break;
	case VIRTIO_CR_CFGVEC:
		base->msix_cfg_idx = value;
		break;
	case VIRTIO_CR_QVEC:
		if (base->curq >= vops->nvq)
			goto bad_qindex;
		vq = &base->queues[base->curq];
		vq->msix_idx = value;
		break;
	}
	goto done;

bad_qindex:
	fprintf(stderr,
	    "%s: write config reg %s: curq %d >= max %d\r\n",
	    name, cr->name, base->curq, vops->nvq);
done:
	if (base->mtx)
		pthread_mutex_unlock(base->mtx);
}
